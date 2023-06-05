#include "client.h"
#include "communicator.h"
#include "http1.h"
#include "utils.h"

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

/*
 * Create connection socket to `node`:`service`
 * By default IPv4 is used.
 * Set `is_ipv6` to use IPv6 instead
 */
static int create_connection_sock(char* node, char* service, _Bool is_ipv6) {
    int gai_err = 0;
    struct addrinfo hint = {
        .ai_family = is_ipv6 ? AF_INET6 : AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res = NULL;
    gai_err = getaddrinfo(NULL, service, &hint, &res);
    ASSERT_RETURN(gai_err == 0, -1, "gai error: %s\n", gai_strerror(gai_err));
    ASSERT_RETURN(res != NULL, -1, "");

    int sock = -1;
    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
        sock = socket(ai->ai_family, ai->ai_socktype, 0);
        if (sock < 0) {
            continue;
        }
        if (connect(sock, ai->ai_addr, ai->ai_addrlen) < 0) {
            close(sock);
            sock = -1;
            continue;
        }
        break;
    }
    freeaddrinfo(res);

    return sock;
}

static void print_help(char *executable) {
    printf("Usage: %s HOST PORT CMD [ARGS...]\n", executable);
    puts("");
    puts("Connect to Yorsh daemon running on HOST:PORT and execute CMD with ARGS...");
}

int _main(int argc, char *argv[]) {
    if (argc < 4) {
        print_help(argc >= 1 ? argv[0] : "a.out");
        exit(EXIT_SUCCESS);
    }
    _Bool is_ipv6 = argc >= 4 && strcmp(argv[3], "6") == 0;
    int sock = create_connection_sock(argv[1], argv[2], is_ipv6);
    ASSERT_EXIT(sock >= 0, "Unable to create connection socket\n");

    int res = start_communication(sock, argc - 3, argv + 3);

    return res;
}
