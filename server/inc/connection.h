#pragma once

#include <sys/types.h>

typedef struct connection_info {
    int fd;
    pid_t process_exec;
    pid_t process_handler;
} connection_info;

int new_connection(int fd);
