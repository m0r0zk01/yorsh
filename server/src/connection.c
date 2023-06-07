#include "connection.h"
#include "gigaprotocol.h"
#include "http1.h"
#include "utils.h"

#include <ctype.h>
#include <netinet/in.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

static int exec_command(const char *cmd) {
    const char *ptr_cnt = cmd;
    size_t num_args = 0;
    while (*ptr_cnt != '\0') {
        while (*ptr_cnt != '\0' && isspace(*ptr_cnt)) {
            ++ptr_cnt;
        }
        if (*ptr_cnt == '\0') {
            break;
        }
        ++num_args;
        do {
            ++ptr_cnt;
        } while (*ptr_cnt != '\0' && !isspace(*ptr_cnt));
    }
    if (num_args == 0) {
        return -1;
    }

    char **argv = calloc(num_args + 1, sizeof(*argv));
    char *cmd_copy = strdup(cmd);
    if (argv == NULL || cmd_copy == NULL) {
        _exit(1);
    }
    argv[num_args] = NULL;
    size_t last_argv = 0;
    char *ptr = cmd_copy;
    while (*ptr != '\0') {
        while (*ptr != '\0' && isspace(*ptr)) {
            *(ptr++) = '\0';
        }
        if (*ptr == '\0') {
            break;
        }
        argv[last_argv++] = ptr;
        do {
            ++ptr;
        } while (*ptr != '\0' && !isspace(*ptr));
    }

    execvp(argv[0], argv);
    _exit(1);
}

volatile sig_atomic_t connection_sig;

static void sigchld_handler(int) {
    close(connection_sig);
    wait(NULL);
}

static int start_spawn_handler(int connection, char *body) {
    pid_t ppid_before_fork = getpid();
    pid_t pid = fork();
    ASSERT_RETURN(pid >= 0, -1, "fork");
    if (pid > 0) {
        return 0;
    }

    // following is happening in child:
    // exec read from pipe0
    // handler writes to pipe0
    int r = prctl(PR_SET_PDEATHSIG, SIGKILL);
    if (r == -1) {
        perror("prctl");
        exit(1);
    }
    if (getppid() != ppid_before_fork) {
        LOG("wtf1\n");
        exit(EXIT_FAILURE);
    }

    int pipe0[2];
    int pipe_res = pipe(pipe0);
    ASSERT_PERROR__EXIT(pipe_res >= 0 && pipe_res >= 0, "pipe");

    connection_sig = connection;
    struct sigaction sa = {.sa_handler = sigchld_handler, .sa_flags = SA_RESTART};
    sigaction(SIGCHLD, &sa, NULL);

    ppid_before_fork = getpid();
    pid_t exec_pid = fork();
    ASSERT_PERROR__EXIT(exec_pid >= 0, "fork");

    if (exec_pid == 0) {
        int r = prctl(PR_SET_PDEATHSIG, SIGKILL);
        if (r == -1) {
            perror("prctl");
            exit(1);
        }
        if (getppid() != ppid_before_fork) {
            LOG("wtf\n");
            exit(EXIT_FAILURE);
        }

        close(pipe0[1]);
        dup2(pipe0[0], STDIN_FILENO);
        dup2(connection, STDOUT_FILENO);
        close(connection);
        close(pipe0[0]);

        exec_command(body);
    }
    close(pipe0[0]);

    // process gigaprotocol messages from client
    int signal_in_process = 0;
    int read_res = 0;
    unsigned char arr[4];
    LOG("Starting event loop\n");
    while (1) {
        int read_res = read_n(arr, 4, connection);
        if (read_res <= 0) { break; }
        uint32_t type = giga_load32(arr);
        if (type == MESSAGE) {
            read_res = read_n(arr, 4, connection);
            if (read_res <= 0) { break; }
            uint32_t length = giga_load32(arr);
            unsigned char *buf = calloc(length, 1);
            read_res = read_n(buf, length, connection);
            if (read_res <= 0) { break; }
            write_n(buf, length, pipe0[1]);
            free(buf);
        } else if (type == SIGNAL) {
            read_res = read_n(arr, 4, connection);
            if (read_res <= 0) { break; }
            uint32_t signal = giga_load32(arr);
            LOG("sending signal %d to child %d\n", signal, exec_pid);
            LOG("SIGING is %d\n", SIGINT);
            char msg_to_client[] = "\nCaught signal, redirecting it to running process\n";
            write_n(msg_to_client, sizeof(msg_to_client), connection);
            kill(exec_pid, signal);
        } else if (type == EOF) {
            LOG("Got EOF\n");
            close(pipe0[1]);
            pipe0[1] = -1;
        }
    }
    LOG("exited event loop\n");

    close(connection);
    if (pipe0[1] != -1) {
        close(pipe0[1]);
    }
    kill(exec_pid, SIGKILL);
    wait(NULL);

    LOG("exec child finished\n");
    _exit(0);
}

static void handle_command(int connection, http1_request *req) {
    http1_response resp;
    http1_init_response(&resp);
    resp.status_code = 200;

    char *msg;
    size_t msg_len;
    if (strcmp(req->path.data[0], "ping") == 0) {
        msg = copy_string("pong");
        msg_len = strlen(msg);
    } else if (strcmp(req->path.data[0], "http-echo") == 0) {
        http1_dumps_request(&msg, &msg_len, req);
    }

    http1_set_body_response(msg, msg_len, &resp);
    free(msg);

    char to_str[100] = {0};
    sprintf(to_str, "%zu", resp.body_len);
    http1_add_header_response("Content-Length", to_str, &resp);

    char *buf;
    size_t size;
    http1_dumps_response(&buf, &size, &resp);
    write_n(buf, size, connection);
    free(buf);
    http1_free_response(&resp);
}

int new_connection(int sock) {
    int connection = accept(sock, NULL, NULL);
    ASSERT_PERROR_RETURN(connection >= 0, -1, "accept4");

    http1_request req;
    int res = http1_read_request(connection, &req);
    ASSERT_RETURN(res >= 0, -1, "Error while recieving command from client\n");

    ASSERT_RETURN(req.path.size != 0, -1, "No command");

    if (strcmp(req.path.data[0], "spawn") == 0) {
        start_spawn_handler(connection, req.body);
    } else {
        handle_command(connection, &req);
    }

    http1_free_request(&req);
    close(connection);
    return 0;
}
