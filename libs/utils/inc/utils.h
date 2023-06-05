#pragma once

#include "stddef.h"

#define VA_ARGS(...) , ##__VA_ARGS__

#define ASSERT_PERROR_EXIT(cond, msg)                       \
do {                                                        \
    if (!(cond)) {                                          \
        perror(msg);                                        \
        exit(EXIT_FAILURE);                                 \
    }                                                       \
} while (0)

#define ASSERT_PERROR__EXIT(cond, msg)                      \
do {                                                        \
    if (!(cond)) {                                          \
        perror(msg);                                        \
        _exit(EXIT_FAILURE);                                \
    }                                                       \
} while (0)

#define ASSERT_PERROR_RETURN(cond, val, msg)                \
do {                                                        \
    if (!(cond)) {                                          \
        perror(msg);                                        \
        return (val);                                       \
    }                                                       \
} while (0)

#define ASSERT_EXIT(cond, format, ...)                      \
do {                                                        \
    if (!(cond)) {                                          \
        fprintf(stderr, (format) VA_ARGS(__VA_ARGS__));     \
        exit(EXIT_FAILURE);                                 \
    }                                                       \
} while (0)

#define ASSERT_RETURN(cond, val, format, ...)               \
do {                                                        \
    if (!(cond)) {                                          \
        fprintf(stderr, (format) VA_ARGS(__VA_ARGS__));     \
        return (val);                                       \
    }                                                       \
} while (0)

#define LOG(format, ...)                                    \
do {                                                        \
    fprintf(stdout, "[INFO] ");                             \
    fprintf(stdout, format VA_ARGS(__VA_ARGS__));           \
} while (0)

char *copy_substring(char *begin, char *end);
char *copy_string(char *begin);

int write_n(unsigned char *buf, size_t n, int fd);
int read_n(unsigned char *buf, size_t n, int fd);
