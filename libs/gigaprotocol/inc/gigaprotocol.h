#pragma once

#include <stddef.h>
#include <stdint.h>

/*
 * Package structure:
 *
 * [uint32 type] [uint32 length of data] [data]
 */

/**/
typedef enum {
    MESSAGE,
    SIGNAL
} giga_t;

void giga_dump32(uint32_t n, unsigned char arr[4]);
uint32_t giga_load32(unsigned char arr[4]);

void giga_create_message(unsigned char **dest, unsigned char *buf, size_t n);
void giga_create_signal(unsigned char **dest, int signal);
