#include "http1.h"
#include "utils.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { MAX_STATUS_CODE_LENGTH = 5, HTTP_PROTOCOL_LENGTH = 10, ADDITION = 100, MAX_HEADER_SIZE = 4095 };

const char *http1_method_strings[] = {
    FOREACH_METHOD(TO_STRING)
};

char *str(char *s) {
    size_t size = strlen(s);
    char *res = calloc(size + 1, 1);
    memcpy(res, s, size);
    res[size] = '\0';
    return res;
}

void vector_deleter_http1_header(vector_http1_header *vec) {
    for (size_t i = 0; i < vec->size; ++i) {
        free(vec->data[i].key);
        free(vec->data[i].value);
    }
}

void vector_deleter_string(vector_string *vec) {
    for (size_t i = 0; i < vec->size; ++i) {
        free(vec->data[i]);
    }
}

void vector_deleter_http1_cgi(vector_http1_cgi *vec) {
    for (size_t i = 0; i < vec->size; ++i) {
        free(vec->data[i].key);
        vector_free_string(&vec->data[i].vals, vector_deleter_string);
    }
}

http1_method http1_method_from_string(char *str, size_t size) {
    for (int i = 0; i < COUNT; ++i) {
        if (strncmp(http1_method_strings[i], str, size) == 0) {
            return (http1_method)i;
        }
    }
    return UNKNOWN;
}

char *status_code_to_string(int status_code) {
    switch (status_code) {
        case 200:
            return "OK";
        case 404:
            return "Not Found";
        default:
            return "Unknown";
    }
}

void http1_init_request(http1_request *req) {
    req->method = UNKNOWN;
    req->body = str("");
    req->body_len = 0;
    vector_create_string(&req->path);
    vector_create_http1_cgi(&req->cgis);
    vector_create_http1_header(&req->headers);
}

void http1_init_response(http1_response *resp) {
    resp->status_code = 0;
    resp->status_string[0] = '\0';
    resp->body = str("");
    resp->body_len = 0;
    vector_create_http1_header(&resp->headers);
}

/*
 * Find next occurence of `c` in [`begin`; `end`)
 */
static char *find_next(char *begin, char *end, char c) {
    while (begin < end && *begin != c) {
        ++begin;
    }
    return begin;
}

/*
 * Fill *`cgis` from string of form ?key1=val1&key2=val2&...
*/
static int fill_cgi(char *begin, char *end, vector_http1_cgi *cgis) {
    vector_create_http1_cgi(cgis);
    char *cur = begin;
    char *next = NULL;
    ++cur;
    while (cur + 1 < end) {
        next = find_next(cur, end, '&');
        char *eq = find_next(cur, next, '=');
        if (eq == next) {
            vector_free_http1_cgi(cgis, vector_deleter_http1_cgi);
            fprintf(stderr, "Invalid cgi string: missing equality sign\n");
            return -1;
        }
        _Bool filled = 0;
        for (size_t i = 0; i < cgis->size; ++i) {
            if (strncmp(cgis->data[i].key, cur, eq - cur) == 0) {
                filled = 1;
                vector_push_back_string(&cgis->data[i].vals, copy_substring(eq + 1, next));
            }
        }
        if (!filled) {
            http1_cgi new_cgi;
            vector_create_string(&new_cgi.vals);
            new_cgi.key = copy_substring(cur, eq);
            vector_push_back_string(&new_cgi.vals, copy_substring(eq + 1, next));
            vector_push_back_http1_cgi(cgis, new_cgi);
        }
        cur = next + 1;
    }
    return 0;
}

/*
 * Fill *`path` from string of form /aba/caba/...
*/
static int fill_path(char *begin, char *end, vector_string *path) {
    vector_create_string(path);
    char *cur = begin + 1;
    char *next = NULL;
    while (cur < end) {
        next = find_next(cur, end, '/');
        vector_push_back_string(path, copy_substring(cur, next));
        cur = next + 1;
    }
    return 0;
}

/*
 * Fill *`headers` from string of form key1: value1\nkey2: value2...
*/
static int fill_headers(char *begin, char *end, vector_http1_header *headers, char **curr) {
    vector_create_http1_header(headers);
    char *cur = begin;
    char *next = NULL;
    while (1) {
        next = find_next(cur, end, '\r');
        if (next == end || next + 1 == end || *(next + 1) != '\n') {
            vector_free_http1_header(headers, vector_deleter_http1_header);
            fprintf(stderr, "Invalid headers string: missing CRLF\n");
            return -1;
        }
        if (cur == next) {
            break;
        }
        char *colon = find_next(cur, next, ':');
        if (colon == next || *(colon + 1) != ' ') {
            vector_free_http1_header(headers, vector_deleter_http1_header);
            fprintf(stderr, "Invalid headers string: missing \": \"\n");
            return -1;
        }

        http1_header new_header;
        new_header.key = copy_substring(cur, colon);
        new_header.key_len = colon - cur;
        new_header.value = copy_substring(colon + 2, next);
        new_header.value_len = next - (colon + 2);
        vector_push_back_http1_header(headers, new_header);
        cur = next + 2;
    }
    *curr = cur;
    return 0;
}

int http1_parse_request_headers(char *data, size_t size, http1_request *req) {
    char *end = data + size;
    char *cur = data, *next = data;

    // parse method
    next = find_next(cur, end, ' ');
    ASSERT_RETURN(next != end, -1, "No space after method in HTTP request\n");
    req->method = http1_method_from_string(cur, next - cur);
    cur = next + 1;

    // parse cgi
    next = find_next(cur, end, ' ');
    ASSERT_RETURN(next != end, -1, "No space after path in HTTP request\n");
    char *cgi_begin = find_next(cur, end, '?');
    int fill_cgi_res = fill_cgi(cgi_begin, next, &req->cgis);
    ASSERT_RETURN(fill_cgi_res == 0, -1, "Filling CGI failed\n");

    // parse path
    int fill_path_res = fill_path(cur, cgi_begin < next ? cgi_begin : next, &req->path);
    if (fill_path_res != 0) {
        vector_free_http1_cgi(&req->cgis, vector_deleter_http1_cgi);
        fprintf(stderr, "Failed parsing path\n");
        return -1;
    }

    // skip protocol
    next = find_next(cur, end, '\r');
    if (next == end || next + 1 == end || *(next + 1) != '\n') {
        vector_free_http1_cgi(&req->cgis, vector_deleter_http1_cgi);
        fprintf(stderr, "MIssing CRLF after protocol\n");
        return -1;
    }

    // parse headers line by line until empty one
    cur = next + 2;
    int fill_headers_res = fill_headers(cur, end, &req->headers, &cur);
    if (fill_headers_res != 0) {
        vector_free_http1_cgi(&req->cgis, vector_deleter_http1_cgi);
        vector_free_string(&req->path, vector_deleter_string);
        fprintf(stderr, "Failed parsing HTTP request headers\n");
        return -1;
    }

    // pasrse body
    // ++cur;
    // req->body = copy_substring(cur, end);
    // req->body_len = end - cur;
    return 0;
}

int http1_parse_response_headers(char *data, size_t size, http1_response *resp) {
    char *end = data + size;
    char *cur = data, *next = data;

    // skip HTTP protocol
    next = find_next(cur, end, ' ');
    ASSERT_RETURN(next != end, -1, "No space after HTTP protocol\n");

    // parse status_code
    cur = next + 1;
    next = find_next(cur, end, ' ');
    ASSERT_RETURN(next != end, -1, "No space after HTTP satus_code\n");
    errno = 0;
    resp->status_code = strtol(cur, NULL, 10);
    ASSERT_RETURN(errno == 0, -1, "Failed converting status_code to integer\n");

    // parse status_string
    cur = next + 1;
    next = find_next(cur, end, '\r');
    ASSERT_RETURN(next != end && next + 1 != end && *(next + 1) == '\n', -1, "No CRLF after HTTP satus_string\n");
    memcpy(resp->status_string, cur, sizeof(resp->status_string) - 1);
    resp->status_string[next - cur] = '\0';

    // parse headers
    cur = next + 2;
    int fill_headers_res = fill_headers(cur, end, &resp->headers, &cur);
    ASSERT_RETURN(fill_headers_res == 0, -1, "Failed parsing HTTP response headers\n");

    // parse body
    // ++cur;
    // resp->body = copy_substring(cur, end);
    // resp->body_len = end - cur;
    return 0;
}

static size_t calc_request_length(http1_request *req) {
    size_t res = 0;
    res += sizeof(http1_method_strings[req->method]) + 1;
    for (size_t i = 0; i < req->path.size; ++i) {
        res += 1 + strlen(req->path.data[i]);
    }
    res += 1 + HTTP_PROTOCOL_LENGTH + 2;

    for (size_t i = 0; i < req->headers.size; ++i) {
        res += req->headers.data[i].key_len + 2 + req->headers.data[i].value_len + 2;
    }
    res += 2;

    res += req->body_len + ADDITION;

    return res;
}

int http1_dumps_request(char **buf, size_t *size, http1_request *req) {
    *size = calc_request_length(req);
    *buf = calloc(*size, 1);
    char *ptr = *buf;

    ptr += sprintf(ptr, "%s ", http1_method_strings[req->method]);
    for (size_t i = 0; i < req->path.size; ++i) {
        ptr += sprintf(ptr, "/%s", req->path.data[i]);
    }
    if (req->cgis.size) {
        ptr += sprintf(ptr, "?");
    }
    for (size_t i = 0; i < req->cgis.size; ++i) {
        for (size_t j = 0; j < req->cgis.data[i].vals.size; ++j) {
            ptr += sprintf(ptr, "%s=%s", req->cgis.data[i].key, req->cgis.data[i].vals.data[j]);
            if (j + 1 != req->cgis.data[i].vals.size) {
                ptr += sprintf(ptr, "&");
            }
        }
        if (req->cgis.data[i].vals.size > 0 && i + 1 != req->cgis.size) {
            ptr += sprintf(ptr, "&");
        }
    }
    ptr += sprintf(ptr, " HTTP/1.1\r\n");

    for (size_t i = 0; i < req->headers.size; ++i) {
        ptr += sprintf(ptr, "%s: %s\r\n", req->headers.data[i].key, req->headers.data[i].value);
    }
    ptr += sprintf(ptr, "\r\n");

    ptr += sprintf(ptr, "%s", req->body);

    *size = ptr - *buf;

    return 0;
}

static size_t calc_response_length(http1_response *resp) {
    size_t res = 0;
    res += HTTP_PROTOCOL_LENGTH + 1;
    res += MAX_STATUS_CODE_LENGTH + 1;
    res += sizeof(resp->status_string) + 2;

    for (size_t i = 0; i < resp->headers.size; ++i) {
        res += resp->headers.data[i].key_len + 2 + resp->headers.data[i].value_len + 2;
    }
    res += 2;

    res += resp->body_len + ADDITION;

    return res;
}

int http1_dumps_response(char **buf, size_t *size, http1_response *resp) {
    *size = calc_response_length(resp);
    *buf = calloc(*size, 1);
    char *ptr = *buf;

    ptr += sprintf(ptr, "HTTP/1.1 %d %s\r\n", resp->status_code, resp->status_string);

    for (size_t i = 0; i < resp->headers.size; ++i) {
        ptr += sprintf(ptr, "%s: %s\r\n", resp->headers.data[i].key, resp->headers.data[i].value);
    }
    ptr += sprintf(ptr, "\r\n");

    ptr += sprintf(ptr, "%s", resp->body);

    *size = ptr - *buf;

    return 0;
}

void http1_free_request(http1_request *req) {
    vector_free_string(&req->path, vector_deleter_string);
    vector_free_http1_cgi(&req->cgis, vector_deleter_http1_cgi);
    vector_free_http1_header(&req->headers, vector_deleter_http1_header);
    free(req->body);
}

void http1_free_response(http1_response *resp) {
    vector_free_http1_header(&resp->headers, vector_deleter_http1_header);
    free(resp->body);
}

static int http1_read(int sock, void *v, _Bool is_request) {
    char headers[MAX_HEADER_SIZE + 1];
    size_t readd = 0;
    while (1) {
        if (read(sock, headers + readd, 1) <= 0) {
            return -1;
        }
        ++readd;
        if (readd >= 4 && strncmp(headers + readd - 4, "\r\n\r\n", 4) == 0) {
            break;
        }
        if (readd == MAX_HEADER_SIZE) {
            break;
        }
    }
    if (is_request) {
        http1_parse_request_headers(headers, readd, v);
    } else {
        http1_parse_response_headers(headers, readd, v);
    }
    long long content_length = 0;
    if (is_request) {
        http1_request *req = v;
        for (size_t i = 0; i < req->headers.size; ++i) {
            if (strcmp(req->headers.data[i].key, "Content-Length") == 0) {
                content_length = strtoll(req->headers.data[i].value, NULL, 10);
                break;
            }
        }
        req->body = calloc(content_length + 1, 1);
        req->body_len = content_length;
        if ((readd + content_length > MAX_HEADER_SIZE) || (content_length > 0 && read(sock, req->body, content_length) <= 0)) {
            if (is_request) {
                http1_free_request(v);
            } else {
                http1_free_response(v);
            }
            return -1;
        }
    } else {
        http1_response *resp = v;
        for (size_t i = 0; i < resp->headers.size; ++i) {
            if (strcmp(resp->headers.data[i].key, "Content-Length") == 0) {
                content_length = strtoll(resp->headers.data[i].value, NULL, 10);
                break;
            }
        }
        resp->body = calloc(content_length + 1, 1);
        resp->body_len = content_length;
        if ((readd + content_length > MAX_HEADER_SIZE) || (content_length > 0 && read(sock, resp->body, content_length) <= 0)) {
            if (is_request) {
                http1_free_request(v);
            } else {
                http1_free_response(v);
            }
            return -1;
        }
    }
    return 0;
}

int http1_read_request(int sock, http1_request *req) {
    return http1_read(sock, req, 1);
}

int http1_read_response(int sock, http1_response *resp) {
    return http1_read(sock, resp, 0);
}

void http1_add_header_request(char *key, char *value, http1_request *req) {
    http1_header content_length;

    content_length.key = copy_string(key);
    content_length.key_len = strlen(content_length.key);

    content_length.value = copy_string(value);
    content_length.value_len = strlen(content_length.value);

    vector_push_back_http1_header(&req->headers, content_length);
}

void http1_add_header_response(char *key, char *value, http1_response *resp) {
    http1_header content_length;

    content_length.key = copy_string(key);
    content_length.key_len = strlen(content_length.key);

    content_length.value = copy_string(value);
    content_length.value_len = strlen(content_length.value);

    vector_push_back_http1_header(&resp->headers, content_length);
}

void http1_set_body_request(char *body, size_t body_len, http1_request *req) {
    free(req->body);
    req->body = copy_substring(body, body + body_len);
    req->body_len = body_len;
}

void http1_set_body_response(char *body, size_t body_len, http1_response *resp) {
    free(resp->body);
    resp->body = copy_substring(body, body + body_len);
    resp->body_len = body_len;
}
