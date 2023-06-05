#include "http1.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_parsing_request() {
    char *req = "GET /kek/lol?key1=val1&arr=1&arr=2 HTTP/1.1\r\nKEK: LOL\r\nContent-Length: 100\r\n\r\naboba";
    http1_request parsed;
    http1_parse_request_headers(req, strlen(req), &parsed);

    ASSERT_EXIT(parsed.method == GET, "Method: expected GET, found: %s\n", http1_method_strings[parsed.method]);

    ASSERT_EXIT(parsed.path.size == 2, "Path: expected size 2, found: %zu\n", parsed.path.size);
    ASSERT_EXIT(strcmp(parsed.path.data[0], "kek") == 0, "Path: expected kek, found: %s\n", parsed.path.data[0]);
    ASSERT_EXIT(strcmp(parsed.path.data[1], "lol") == 0, "Path: expected lol, found: %s\n", parsed.path.data[1]);

    ASSERT_EXIT(parsed.cgis.size == 2, "CGIs: expected size 2, found: %zu\n", parsed.cgis.size);
    ASSERT_EXIT(strcmp(parsed.cgis.data[0].key, "key1") == 0, "CGIs: expected first key key1, found: %s\n", parsed.cgis.data[0].key);
    ASSERT_EXIT(strcmp(parsed.cgis.data[0].vals.data[0], "val1") == 0, "CGIs: expected first vals[0] val1, found: %s\n", parsed.cgis.data[0].vals.data[0]);
    ASSERT_EXIT(strcmp(parsed.cgis.data[1].key, "arr") == 0, "CGIs: expected second key arr, found: %s\n", parsed.cgis.data[1].key);
    ASSERT_EXIT(strcmp(parsed.cgis.data[1].vals.data[0], "1") == 0, "CGIs: expected second vals[0] 1, found: %s\n", parsed.cgis.data[1].vals.data[0]);
    ASSERT_EXIT(strcmp(parsed.cgis.data[1].vals.data[1], "2") == 0, "CGIs: expected second vals[1] 2, found: %s\n", parsed.cgis.data[1].vals.data[1]);

    ASSERT_EXIT(parsed.headers.size == 2, "Headers: expected size 2, found: %zu\n", parsed.headers.size);
    ASSERT_EXIT(strcmp(parsed.headers.data[0].key, "KEK") == 0, "Headers: expected key KEK, found %s\n", parsed.headers.data[0].key);
    ASSERT_EXIT(strcmp(parsed.headers.data[0].value, "LOL") == 0, "Headers: expected value LOL, found %s\n", parsed.headers.data[0].value);
    ASSERT_EXIT(strcmp(parsed.headers.data[1].key, "Content-Length") == 0, "Headers: expected key Content-Length, found %s\n", parsed.headers.data[1].key);
    ASSERT_EXIT(strcmp(parsed.headers.data[1].value, "100") == 0, "Headers: expected value 100, found %s\n", parsed.headers.data[1].value);

    // ASSERT_EXIT(parsed.body_len == 5, "Body: expected length 5, found: %zu\n", parsed.body_len);
    // ASSERT_EXIT(strcmp(parsed.body, "aboba") == 0, "Body: expected aboba, found: %s\n", parsed.body);

    http1_free_request(&parsed);

    LOG("passed test_parsing_request\n");
}

void test_parsing_response() {
    char *resp = "HTTP/1.1 200 OK\r\nKEK: LOL\r\nContent-Length: 100\r\n\r\naboba";
    http1_response parsed;
    http1_parse_response_headers(resp, strlen(resp), &parsed);

    ASSERT_EXIT(parsed.status_code == 200, "status_code: expected 200, found: %d\n", parsed.status_code);
    ASSERT_EXIT(strcmp(parsed.status_string, "OK") == 0, "status_string: expected OK, found: %s\n", parsed.status_string);

    ASSERT_EXIT(parsed.headers.size == 2, "Headers: expected size 2, found: %zu\n", parsed.headers.size);
    ASSERT_EXIT(strcmp(parsed.headers.data[0].key, "KEK") == 0, "Headers: expected key KEK, found %s\n", parsed.headers.data[0].key);
    ASSERT_EXIT(strcmp(parsed.headers.data[0].value, "LOL") == 0, "Headers: expected value LOL, found %s\n", parsed.headers.data[0].value);
    ASSERT_EXIT(strcmp(parsed.headers.data[1].key, "Content-Length") == 0, "Headers: expected key Content-Length, found %s\n", parsed.headers.data[1].key);
    ASSERT_EXIT(strcmp(parsed.headers.data[1].value, "100") == 0, "Headers: expected value 100, found %s\n", parsed.headers.data[1].value);

    // ASSERT_EXIT(parsed.body_len == 5, "Body: expected length 5, found: %zu\n", parsed.body_len);
    // ASSERT_EXIT(strcmp(parsed.body, "aboba") == 0, "Body: expected aboba, found: %s\n", parsed.body);

    http1_free_response(&parsed);

    LOG("passed test_parsing_response\n");
}

void test_dumping_request() {
    http1_request req;

    req.method = POST;

    vector_create_string(&req.path);
    vector_push_back_string(&req.path, str("a"));
    vector_push_back_string(&req.path, str("bb"));
    vector_push_back_string(&req.path, str("ccc"));

    vector_create_http1_cgi(&req.cgis);
    http1_cgi cgi1;
    cgi1.key = str("arr");
    cgi1.key_len = 3;
    vector_create_string(&cgi1.vals);
    vector_push_back_string(&cgi1.vals, str("val1"));
    vector_push_back_string(&cgi1.vals, str("val2"));
    vector_push_back_http1_cgi(&req.cgis, cgi1);
    http1_cgi cgi2;
    cgi2.key = str("kek");
    cgi2.key_len = 3;
    vector_create_string(&cgi2.vals);
    vector_push_back_string(&cgi2.vals, str("lol"));
    vector_push_back_http1_cgi(&req.cgis, cgi2);

    vector_create_http1_header(&req.headers);
    http1_header header1;
    header1.key = str("Content-Length");
    header1.key_len = 14;
    header1.value = str("100");
    header1.value_len = 3;
    vector_push_back_http1_header(&req.headers, header1);

    req.body = str("aboba");
    req.body_len = 5;

    char *expected = "POST /a/bb/ccc?arr=val1&arr=val2&kek=lol HTTP/1.1\r\nContent-Length: 100\r\n\r\naboba";

    char *buf;
    size_t size;
    http1_dumps_request(&buf, &size, &req);

    ASSERT_EXIT(size == 79, "Size: expected 79, found: %zu\n", size);
    ASSERT_EXIT(strcmp(expected, buf) == 0, "Body: expected:\n---\n%s\n---, found:\n---\n%s\n---", expected, buf);

    free(buf);
    http1_free_request(&req);

    LOG("passed test_dumping_request\n");
}

void test_dumping_response() {
    http1_response resp;

    resp.status_code = 200;
    memcpy(resp.status_string, "OK\0", 3);

    vector_create_http1_header(&resp.headers);
    http1_header header1;
    header1.key = str("Content-Length");
    header1.key_len = 14;
    header1.value = str("100");
    header1.value_len = 3;
    vector_push_back_http1_header(&resp.headers, header1);

    resp.body = str("aboba");
    resp.body_len = 5;

    char *expected = "HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\naboba";

    char *buf;
    size_t size;
    http1_dumps_response(&buf, &size, &resp);

    ASSERT_EXIT(size == 45, "Size: expected 45, found: %zu\n", size);
    ASSERT_EXIT(strcmp(expected, buf) == 0, "Body: expected:\n---\n%s\n---, found:\n---\n%s\n---", expected, buf);

    free(buf);
    http1_free_response(&resp);

    LOG("passed test_dumping_response\n");
}

int main() {
    test_parsing_request();
    test_parsing_response();
    test_dumping_request();
    test_dumping_response();
}
