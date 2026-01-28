#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "http.h"

#define MAX_HEADER_SIZE 8192      /* 8 KB */ 
#define MAX_REQUEST_SIZE (10*1024*1024) /* 10 MB max total */
#define is_space(x) (x == ' ' ? 1 : 0)

static Server_Settings g_settings;
static pthread_mutex_t server_settings_mutex = PTHREAD_MUTEX_INITIALIZER;

Server_Settings http_get_server_settings(void)
{
    Server_Settings copy;
    pthread_mutex_lock(&server_settings_mutex);
    copy = g_settings;
    pthread_mutex_unlock(&server_settings_mutex);
    return copy;
}

void http_set_server_settings(Server_Settings settings_in)
{
    pthread_mutex_lock(&server_settings_mutex);
    memcpy(&g_settings, &settings_in, sizeof(Server_Settings));
    pthread_mutex_unlock(&server_settings_mutex);
}

static int parse_headers(char* header_data, HttpRequest *req)
{
    char *p, *line, *query;

    req->content_length = -1;

    /* first req line */
    line = strtok(header_data, "\r\n");
    if (!line) return -1;
    sscanf(line, "%7s %255s", req->method, req->path);
    query = strstr(req->path, "?");
    if (query)
    {
        *query = 0;
        query += 1;
        req->query = malloc(strlen(query) + 1);
        snprintf(req->query, strlen(query) + 1, "%s", query);
    }

    /* parse other headers */
    while ((line = strtok(NULL, "\r\n")) != NULL && *line != '\0')
    {
        if (strncasecmp(line, "Host:", 5) == 0)
        {
            snprintf(req->host, sizeof(req->host), "%s", line + 5);
            p = req->host;
            while (is_space((unsigned char)*p)) p++;
            memmove(req->host, p, strlen(p) + 1);

        } else if (strncasecmp(line, "Content-Length:", 15) == 0)
            req->content_length = strtol(line + 15, NULL, 10);
        else if (strncasecmp(line, "Connection:", 11) == 0)
        {
            if (strstr(line + 11, "keep-alive"))
                req->keep_alive = 1;
        }
        else if (strncasecmp(line, "Cookie:", 7) == 0)
        {
            p = line + 7;
            while (is_space((unsigned char)*p)) p++;

            req->cookie = malloc(strlen(p) + 1);
            if (!req->cookie) return -1;
            snprintf(req->cookie, strlen(p) + 1, "%s", p);
        }
    }
    return 1;
}

static ssize_t read_http_header(tcp_args args, char *buffer, size_t buf_size, char** end_of_header)
{
    size_t total = 0;
    ssize_t n;

    while (total < buf_size)
    {
        if (args.ssl)
            n = secure_recv(args.ssl, buffer + total, buf_size - total);
        else
            n = recv(args.client_fd, buffer + total, buf_size - total, 0);
        if (n <= 0)
            return -1;

        total += n;
        buffer[total] = '\0';

        if ((*end_of_header = strstr(buffer, "\r\n\r\n")) != NULL)
        {
            *end_of_header += 4;
            return total;
        }
    }

    /* headers too large */
    return -2;
}

static ssize_t read_http_content(tcp_args args, char *buffer, size_t buf_size, size_t content_size)
{
    size_t total = 0;
    ssize_t n;

    if (content_size > (buf_size - 1))
        return -2;

    while (total < content_size)
    {
        if (args.ssl)
            n = secure_recv(args.ssl, buffer + total, content_size - total);
        else
            n = recv(args.client_fd, buffer + total, content_size - total, 0);
        if (n <= 0)
            return -1;

        total += n;
        buffer[total] = '\0';
    }

    return total;
}

int http_make_basic_headers(HttpResponse response, char *buffer, int max_size)
{
    int parsed;
    if (response.content_length)
        parsed = snprintf(buffer, max_size, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\nConnection: %s\r\n\r\n", response.return_code, response.msg_code, response.content_type, response.content_length, response.connection);
    else
        parsed = snprintf(buffer, max_size, "HTTP/1.1 %d %s\r\nConnection: %s\r\n\r\n", response.return_code, response.msg_code, response.connection);
    return parsed;
}

char *get_content_type(char *path)
{
    char *format;

    format = strstr(path, ".");
    if (!format)
        return "text/html";
    format += 1;

    if (!strcmp(format, "html")) return "text/html";
    if (!strcmp(format, "htm")) return "text/html";
    if (!strcmp(format, "css")) return "text/css";
    if (!strcmp(format, "js")) return "application/javascript";
    if (!strcmp(format, "json")) return "application/json";
    if (!strcmp(format, "png")) return "image/png";
    if (!strcmp(format, "jpg")) return "image/jpeg";
    if (!strcmp(format, "jpeg")) return "image/jpeg";
    if (!strcmp(format, "gif")) return "image/gif";

    return "text/html"; /* unkown */
}

static void free_request(HttpRequest request)
{
    if (request.content)
        free(request.content);
    request.content_length = 0;
    if (request.cookie)
        free(request.cookie);
    if (request.query)
        free(request.query);
}

void *http_routine(void *thr_arg)
{
    tcp_args args = *((tcp_args*)thr_arg);
    HttpRequest request = { 0 };
    HttpExtraArgs *extra_args;
    char *client_req = NULL, *header_end;
    int total_header_bytes; /* could include some body */
    ssize_t s;

    free(thr_arg);

    client_req = malloc(MAX_REQUEST_SIZE + 1);
    if (!client_req)
    {
        WARN("Could not malloc space for client http req. client_fd=%d\n", args.client_fd);
        goto client_cleanup;
    }

    total_header_bytes = read_http_header(args, client_req, MAX_HEADER_SIZE, &header_end);
    if (total_header_bytes <= 0)
    {
        WARN("Could not read header data. read_http_headers error %d, client_fd=%d\n", total_header_bytes, args.client_fd);
        goto client_cleanup;
    }

    request.connection_info = args;

    parse_headers(client_req, &request);

    if (request.content_length > 0)
    {
        request.content = calloc(request.content_length + 1, 1);
        if (*header_end)
            snprintf(request.content, request.content_length + 1, "%s", header_end);
        s = strlen(request.content);
        read_http_content(args, request.content + s, MAX_REQUEST_SIZE - MAX_HEADER_SIZE - s, request.content_length - s);
    }

    TRACE("\n\
Method: %s\n\
\tPath: %s\n\
\tHost: %s\n\
\tContent-Length: %ld\n\
\tKeep-Alive: %d\n\
\tContent: (%p)\n", request.method, request.path, request.host, request.content_length, request.keep_alive, request.content);

    extra_args = (HttpExtraArgs*)args.global_args;
    extra_args->client_handler(&request, (HttpExtraArgs*)args.global_args);

client_cleanup:
    if (client_req)
        free(client_req);
    tcp_args_destroy(&args);
    free_request(request);
    return NULL;
}
