#pragma once
#include "tcp.h"

typedef struct HttpResponse
{
    int return_code;
    char msg_code[256];
    char content_type[256];
    size_t content_length;
    char connection[256];
}HttpResponse;

typedef struct HttpRequest
{
    tcp_args connection_info;
    char method[8];
    char path[256];
    char host[256];
    long content_length;
    int keep_alive;
    char *content;
    char *query;
    char *cookie;
}HttpRequest;

typedef struct Server_Settings
{
    char content_folder[256];
    char error_folder[256];
    char index_name[256];

    int port;
    char address[256];
    int max_queue;
}Server_Settings;

typedef struct HttpExtraArgs HttpExtraArgs;
struct HttpExtraArgs
{
    HttpResponse (*client_handler)(HttpRequest *req, HttpExtraArgs *extra_args);
    HttpResponse (*GET_handler)(HttpRequest *req);
    HttpResponse (*POST_handler)(HttpRequest *req);
};

#include "server_err.h"
#include "config_handler.h"

void *http_routine(void *thr_arg);
Server_Settings http_get_server_settings(void);
void http_set_server_settings(Server_Settings settings_in);

int init_http_server(char *settings_json_path);
void http_stop_server();
void http_cleanup_server();
int start_http_server_listen(int server_fd, HttpExtraArgs *extra_arguments, int secure);

/*
    Helpers
*/
int http_make_basic_headers(HttpResponse response, char *buffer, int max_size);
int http_add_header(char *header_buffer, size_t max_size, char *new_headers);
char *get_content_type(char *path);
HttpResponse return_http_error_code(HttpRequest request, int code, char *msg, Server_Settings settings);
HttpResponse handle_default_HTTP_GET(HttpRequest *request);
HttpResponse handle_default_HTTP_POST(HttpRequest *request);
HttpResponse send_http_redirect(HttpRequest* request, char *location, char *addition_headers, Server_Settings settings);
