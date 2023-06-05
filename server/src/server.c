#include "connection.h"
#include "server.h"
#include "utils.h"

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

/*
 * Create listening socket on `service`
 * By default IPv4 is used.
 * Set `is_ipv6` to use IPv6 instead
 */
static int create_listening_sock(char *service, _Bool is_ipv6) {
    int gai_err = 0;
    struct addrinfo hint = {
        .ai_family = is_ipv6 ? AF_INET6 : AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_flags = AI_PASSIVE,
    };
    struct addrinfo *res = NULL;
    gai_err = getaddrinfo(NULL, service, &hint, &res);
    ASSERT_RETURN(gai_err == 0, -1, "gai error: %s\n", gai_strerror(gai_err));
    ASSERT_RETURN(res != NULL, -1, "");

    int sock = -1;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        sock = socket(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (sock < 0) {
            continue;
        }
        int one = 1;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
            close(sock);
            sock = -1;
            continue;
        }
        if (bind(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
            close(sock);
            sock = -1;
            continue;
        }
        if (listen(sock, SOMAXCONN) < 0) {
            close(sock);
            sock = -1;
            continue;
        }
        break;
    }
    freeaddrinfo(res);

    return sock;
}

/*
 * Add `fd` to `epollfd` for reading
 */
static void epoll_add(int epollfd, int fd) {
    struct epoll_event event_tcp = {.events = EPOLLIN | EPOLLET};
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

static void print_help(char *executable) {
    printf("Usage: %s PORT [6]", executable);
    puts("");
    puts("Specify PORT to run server on and an optional '6' character to run server using IPv6. By default IPv4 is used");
}

/*
 * Main server loop
 */
int _main(int argc, char *argv[]) {
    if (argc < 2) {
        print_help(argc >= 1 ? argv[0] : "./a.out");
        exit(EXIT_SUCCESS);
    }

    _Bool is_ipv6 = argc >= 3 && strcmp(argv[2], "6") == 0;
    int sock = create_listening_sock(argv[1], is_ipv6);
    ASSERT_EXIT(sock >= 0, "Unable to create listening socket\n");

    int epollfd = epoll_create1(EPOLL_CLOEXEC);
    ASSERT_PERROR_EXIT(epollfd >= 0, "epoll_create1");
    epoll_add(epollfd, sock);

    // start accepting connections
    LOG("Created server socket & epollfd\n");
    struct epoll_event event;
    while (1) {
        errno = 0;
        // wait for some network event
        if (epoll_wait(epollfd, &event, 1, -1) < 1) {
            printf("error waiting\n");
            if (errno == EINTR) {
                continue;
            }
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        int fd = event.data.fd;
        if (fd == sock) {
            int res = new_connection(fd);
            if (res != 0) {
                LOG("Failed to create new connection\n");
            } else {
                LOG("Successfully created & handled new connection\n");
            }
        } else {
            LOG("unexpected epoll fd\n");
        }
    }

    return 0;
}
