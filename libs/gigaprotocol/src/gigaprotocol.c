#include "gigaprotocol.h"
#include "utils.h"

#include <inttypes.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void giga_dump32(uint32_t n, unsigned char arr[4]) {
    for (int i = 0; i < 4; ++i) {
        arr[i] = n & 0xFF;
        n >>= 8;
    }
}

uint32_t giga_load32(unsigned char arr[4]) {
    return (arr[3] << 24) + (arr[2] << 16) + (arr[1] << 8) + (arr[0] << 0);
}

void giga_create_message(unsigned char **dest, unsigned char *buf, size_t n) {
    *dest = calloc(8 + n, 1);
    unsigned char arr[4];

    giga_dump32(MESSAGE, arr);
    memcpy(*dest, arr, 4);

    giga_dump32(n, arr);
    memcpy(*dest + 4, arr, 4);

    memcpy(*dest + 8, buf, n);
}

void giga_create_signal(unsigned char **dest, int signal) {
    *dest = calloc(12, 1);
    unsigned char arr[4];

    giga_dump32(SIGNAL, arr);
    memcpy(*dest, arr, 4);

    giga_dump32(signal, arr);
    memcpy(*dest + 4, arr, 4);
}

void giga_create_eof(unsigned char **dest) {
    *dest = calloc(12, 1);
    unsigned char arr[4];
    giga_dump32(EOF, arr);
    memcpy(*dest, arr, 4);
}
