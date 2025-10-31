#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "server_err.h"
#include "http.h"
#include "tcp.h"

#define MAX_HEADER_SIZE 8192      /* 8 KB */ 
#define MAX_REQUEST_SIZE (10*1024*1024) /* 10 MB max total */
#define is_space(x) (x == ' ' ? 1 : 0)

static int parse_headers(char* header_data, HttpRequest *req)
{
    char *p, *line;

    req->content_length = -1;

    /* first req line */
    line = strtok(header_data, "\r\n");
    if (!line) return -1;
    sscanf(line, "%7s %255s", req->method, req->path);

    /* parse other headers */
    while ((line = strtok(NULL, "\r\n")) != NULL && *line != '\0')
    {
        if (strncasecmp(line, "Host:", 5) == 0)
        {
            strncpy(req->host, line + 5, sizeof(req->host) - 1);
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
            strcpy(req->cookie, p);
        }
    }
    return 1;
}

static ssize_t read_http_header(int sock, char *buffer, size_t buf_size, char** end_of_header)
{
    size_t total = 0;
    ssize_t n;

    while (total < buf_size)
    {
        n = recv(sock, buffer + total, buf_size - total, 0);
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

static ssize_t read_http_content(int sock, char *buffer, size_t buf_size, size_t content_size)
{
    size_t total = 0;
    ssize_t n;

    if (content_size > (buf_size - 1))
        return -2;

    while (total < content_size)
    {
        n = recv(sock, buffer + total, content_size - total, 0);
        if (n <= 0)
            return -1;

        total += n;
        buffer[total] = '\0';
    }

    return total;
}

int ensure_html_extension(const char *path, char *output, size_t out_size)
{
    strncpy(output, path, out_size - 1);
    output[out_size - 1] = '\0';

    const char *last_slash = strrchr(path, '/');
    const char *last_part = last_slash ? last_slash + 1 : path;

    if (!strchr(last_part, '.'))
    {
        if (strlen(output) + 5 < out_size)
        {
            strcat(output, ".html");
            return 0;
        }
    }
    return 1;
}

int craft_basic_headers(HttpResponse response, char *buffer, int max_size)
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
    return "text/html";
}

void *http_routine(void *thr_arg)
{
    tcp_args args = *((tcp_args*)thr_arg);
    HttpRequest request = { 0 };
    HttpExtraArgs extra_args;
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

    total_header_bytes = read_http_header(args.client_fd, client_req, MAX_HEADER_SIZE, &header_end);
    if (total_header_bytes <= 0)
    {
        WARN("Could not read header data. read_http_headers error %d, client_fd=%d\n", total_header_bytes, args.client_fd);
        goto client_cleanup;
    }

    request.connection_info = args;

    parse_headers(client_req, &request);

    if (request.content_length > 0)
    {
        request.content = calloc(request.content_length, 1);
        if (*header_end)
            strcpy(request.content, header_end);
        s = strlen(request.content);
        read_http_content(args.client_fd, request.content + s, MAX_REQUEST_SIZE - MAX_HEADER_SIZE - s, request.content_length - s);
    }

    TRACE("\n\
Method: %s\n\
Path: %s\n\
Host: %s\n\
Content-Length: %ld\n\
Keep-Alive: %d\n\
Content: (%p)\n", request.method, request.path, request.host, request.content_length, request.keep_alive, request.content);

    extra_args = *(HttpExtraArgs*)args.global_args;
    extra_args.client_handler(&request);

client_cleanup:
    if (client_req)
        free(client_req);
    close(args.client_fd);
    return NULL;
}
