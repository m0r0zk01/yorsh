#include "connection.h"
#include "gigaprotocol.h"
#include "http1.h"
#include "utils.h"

#include <netinet/in.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

static int exec_command(char *argv[]) {
    execvp(argv[0], argv);
    _exit(-1);
}

volatile sig_atomic_t connection_sig;

static void sigchld_handler(int) {
    close(connection_sig);
}

static int start_handler(int connection, char *argv[]) {
    pid_t pid = fork();
    ASSERT_RETURN(pid >= 0, -1, "fork");
    if (pid > 0) {
        return 0;
    }

    // following is happening in child:
    // exec read from pipe0
    // handler writes to pipe0
    int pipe0[2];
    int pipe_res = pipe(pipe0);
    ASSERT_PERROR__EXIT(pipe_res >= 0 && pipe_res >= 0, "pipe");

    connection_sig = connection;
    struct sigaction sa = {.sa_handler = sigchld_handler, .sa_flags = SA_RESTART};
    sigaction(SIGCHLD, &sa, NULL);

    pid_t exec_pid = fork();
    ASSERT_PERROR__EXIT(exec_pid >= 0, "fork");

    if (exec_pid == 0) {
        close(pipe0[1]);
        dup2(pipe0[0], STDIN_FILENO);
        dup2(connection, STDOUT_FILENO);
        close(connection);
        close(pipe0[0]);
        exec_command(argv);
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
            read_res = read_n(arr, 4, connection);  // skip
            if (read_res <= 0) { break; }
            read_res = read_n(arr, 4, connection);  // skip
            if (read_res <= 0) { break; }
            read_res = read_n(arr, 4, connection);
            if (read_res <= 0) { break; }
            uint32_t signal = giga_load32(arr);
            kill(exec_pid, signal);
        }
    }

    LOG("exited event loop\n");
    close(connection);
    close(pipe0[1]);
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

    resp.body_len = msg_len;
    resp.body = calloc(msg_len + 1, 1);
    memcpy(resp.body, msg, resp.body_len);
    free(msg);

    http1_header content_length;
    content_length.key = copy_string("Content-Length");
    content_length.key_len = strlen(content_length.key);
    char to_str[100] = {0};
    sprintf(to_str, "%zu", resp.body_len);
    content_length.value = copy_string(to_str);
    content_length.value_len = strlen(content_length.value);
    vector_push_back_http1_header(&resp.headers, content_length);

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
        // spawn
        vector_push_back_string(&req.path, NULL);
        start_handler(connection, req.path.data + 1);
        wait(NULL);
    } else {
        handle_command(connection, &req);
    }

    http1_free_request(&req);
    close(connection);
    return 0;
}
