#pragma once

#include "vector.h"

#include <stddef.h>
#include <stdio.h>

#define FOREACH_METHOD(foo) \
    foo(GET)                \
    foo(POST)               \
    foo(PUT)                \
    foo(OPTION)             \
    foo(DELETE)             \
    foo(UNKNOWN)            \
    foo(COUNT)              \

#define STR(X) X

#define DECLARE(method)    \
    STR(method),           \

typedef enum http1_method {
    FOREACH_METHOD(DECLARE)
} http1_method;

#define TO_STRING(method)    \
    #method,                 \

extern const char *http1_method_strings[];

http1_method http1_method_from_string(char *str, size_t size);

typedef struct http1_header {
    char *key;
    size_t key_len;

    char *value;
    size_t value_len;
} http1_header;

vector_decl(http1_header)
void vector_deleter_http1_header(vector_http1_header *vec);

typedef char * string;
vector_decl(string)
char *str(char *s);
void vector_deleter_string(vector_string *vec);

typedef struct http1_cgi {
    char *key;
    size_t key_len;

    vector_string vals;
} http1_cgi;

vector_decl(http1_cgi);
void vector_deleter_http1_cgi(vector_http1_cgi *vec);

typedef struct http1_request {
    http1_method method;

    vector_string path;

    vector_http1_cgi cgis;
    vector_http1_header headers;

    char *body;
    size_t body_len;
} http1_request;

typedef struct http1_response {
    int status_code;
    char status_string[20];

    vector_http1_header headers;

    char *body;
    size_t body_len;
} http1_response;

char *status_code_to_string(int status_code);

void http1_init_request(http1_request *req);
void http1_init_response(http1_response *resp);

int http1_parse_request_headers(char *data, size_t size, http1_request *req);
int http1_parse_response_headers(char *data, size_t size, http1_response *resp);

int http1_dumps_request(char **buf, size_t *size, http1_request *req);
int http1_dumps_response(char **buf, size_t *size, http1_response *resp);

void http1_free_request(http1_request *req);
void http1_free_response(http1_response *resp);

int http1_read_request(int sock, http1_request *req);
int http1_read_response(int sock, http1_response *resp);

void http1_add_header_request(char *key, char *value, http1_request *req);
void http1_add_header_response(char *key, char *value, http1_response *resp);

void http1_set_body_request(char *body, size_t body_len, http1_request *req);
void http1_set_body_response(char *body, size_t body_len, http1_response *resp);
