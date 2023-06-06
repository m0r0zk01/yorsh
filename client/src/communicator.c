#include "communicator.h"
#include "gigaprotocol.h"
#include "http1.h"
#include "utils.h"
#include "vector.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <unistd.h>

enum { BUF_SIZE = 4096 };

int send_command(int sock, int argc, char *argv[]) {
    http1_request req;
    http1_init_request(&req);
    req.method = GET;

    _Bool is_spawn = strcmp(argv[0], "spawn") == 0;
    for (int i = 0; i < argc; ++i) {
        vector_push_back_string(&req.path, copy_string(argv[i]));
    }

    char *buf;
    size_t size;
    http1_dumps_request(&buf, &size, &req);
    write_n(buf, size, sock);

    free(buf);
    http1_free_request(&req);

    return is_spawn;
}

/*
 * Add `fd` to `epollfd` for reading
 */
static void epoll_add(int epollfd, int fd, int events) {
    struct epoll_event event_tcp = {.events = events};
    event_tcp.data.fd = fd;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event_tcp);
}

/*
 * Remove `fd` from `epollfd`
 */
static void epoll_remove(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
}

void start_interactive_session(int sock) {
    int epollfd = epoll_create1(EPOLL_CLOEXEC);
    ASSERT_PERROR_EXIT(epollfd >= 0, "epoll_create1");

    static sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    int sigfd = signalfd(-1, &mask, 0);

    epoll_add(epollfd, STDIN_FILENO, EPOLLIN);
    epoll_add(epollfd, sock, EPOLLIN | EPOLLRDHUP);
    epoll_add(epollfd, sigfd, EPOLLIN);

    struct epoll_event event;
    unsigned char buf[BUF_SIZE];
    while (1) {
        errno = 0;
        if (epoll_wait(epollfd, &event, 1, -1) < 1) {
            printf("error waiting\n");
            if (errno == EINTR) {
                continue;
            }
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        if (event.data.fd == STDIN_FILENO) {
            int read_res = read(STDIN_FILENO, buf, BUF_SIZE);
            if (read_res <= 0) {
                shutdown(sock, SHUT_WR);
                continue;
            }
            unsigned char *msg;
            giga_create_message(&msg, buf, read_res);
            write_n(msg, 8 + read_res, sock);
            free(msg);
        } else if (event.data.fd == sock) {
            int read_res = read(sock, buf, BUF_SIZE);
            if (read_res <= 0) {
                break;
            }
            write_n(buf, read_res, STDOUT_FILENO);
        } else if (event.data.fd == sigfd) {
            struct signalfd_siginfo fdsi;
            read(sigfd, &fdsi, sizeof(fdsi));
            unsigned char *msg;
            giga_create_signal(&msg, fdsi.ssi_signo);
            write_n(msg, 12, sock);
            free(msg);
        }
    }
}

void handle_http_response(int sock) {
    http1_response resp;
    int res = http1_read_response(sock, &resp);
    ASSERT_EXIT(res == 0, "Unable to parse HTTP reponse\n");

    printf("%s\n", resp.body);

    http1_free_response(&resp);
}

int start_communication(int sock, int argc, char *argv[]) {
    int cmd_res = send_command(sock, argc, argv);
    ASSERT_EXIT(cmd_res >= 0, "%s\n", "Error while sending command to server\n");
    if (cmd_res > 0) {
        // spawn
        start_interactive_session(sock);
    } else {
        handle_http_response(sock);
    }

    return 0;
}
