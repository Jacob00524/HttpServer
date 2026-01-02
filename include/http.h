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

void *http_routine(void *thr_arg);

/* returns amount written to buffer not including null */
int craft_basic_headers(HttpResponse response, char *buffer, int max_size);

char *get_content_type(char *path);
int ensure_html_extension(const char *path, char *output, size_t out_size);

Server_Settings get_server_settings(void);
void set_server_settings(Server_Settings settings_in);
