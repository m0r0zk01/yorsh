#include "utils.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *copy_substring(char *begin, char *end) {
    size_t len = end - begin;
    char *str = calloc(len + 1, 1);
    memcpy(str, begin, len);
    str[len] = '\0';
    return str;
}

char *copy_string(char *begin) {
    size_t len = strlen(begin);
    char *str = calloc(len + 1, 1);
    memcpy(str, begin, len);
    str[len] = '\0';
    return str;
}

int write_n(unsigned char *buf, size_t n, int fd) {
    size_t written = 0;
    while (written < n) {
        int write_res = write(fd, buf + written, n - written);
        if (write_res <= 0) {
            return write_res;
        }
        written += write_res;
    }
    return written;
}

int read_n(unsigned char *buf, size_t n, int fd) {
    size_t readd = 0;
    while (readd < n) {
        int read_res = read(fd, buf + readd, n - readd);
        if (read_res <= 0) {
            return read_res;
        }
        readd += read_res;
    }
    return readd;
}
