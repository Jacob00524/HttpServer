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
}HttpRequest;

typedef struct HttpExtraArgs
{
    HttpResponse (*client_handler)(HttpRequest* req);
}HttpExtraArgs;

void *http_routine(void *thr_arg);

/* returns amount written to buffer not including null */
int craft_basic_headers(HttpResponse response, char *buffer, int max_size);

char *get_content_type(char *path);
