#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>
#include <errno.h>
#include "config_handler.h"
#include "server_err.h"
#include "tcp.h"
#include "http.h"
#include "server_helpers.h"

char *server_settings_path = "server_settings.json";

HttpResponse handle_http_client(HttpRequest *request)
{
    HttpResponse response = { 0 };
    Server_Settings settings = get_server_settings();
    char path[1024];

    if (!strcmp(request->method, "GET"))
    {
        FILE *requested_file;
        char *file_data;
        size_t file_length;
        char header_data[1024];

        if(!strcmp(request->path, "/"))
            sprintf(path, "%s/%s", settings.content_folder, settings.index_name);
        else
        {
            sprintf(path, "%s%s", settings.content_folder, request->path);
            ensure_html_extension(path, path, sizeof(path)); /* if there is no extension add .html */
        }
        requested_file = fopen(path, "r");
        if (!requested_file)
        {
            response = return_http_error_code(*request, 404, "Not Found", settings);
            return response;
        }
        fseek(requested_file, 0, SEEK_END);
        file_length = ftell(requested_file);
        fseek(requested_file, 0, SEEK_SET);
        file_data = malloc(sizeof(char) * file_length);
        fread(file_data, sizeof(char), file_length, requested_file);
        fclose(requested_file);

        response.return_code = 200;
        strcpy(response.connection, "close");
        strcpy(response.msg_code, "OK");
        strcpy(response.content_type, get_content_type(path));
        response.content_length = file_length;
        
        if (craft_basic_headers(response, header_data, sizeof(header_data)) == sizeof(header_data)-1)
            WARN("It is possilbe not all header data was written.\n");
    
        send(request->connection_info.client_fd, header_data, strlen(header_data), 0);
        send(request->connection_info.client_fd, file_data, response.content_length, 0);
        return response;
    }else
    {
        WARN("Unhandled http method.\n");
    }

    return response;
}

int main()
{
    int server_fd;
    time_t now;
    char time_str[26];
    HttpExtraArgs extra_args;

    now = time(NULL);
    ctime_r(&now, time_str);

    TRACE("Starting server at %s", time_str);
    server_fd = init_http_server(server_settings_path);

    extra_args.client_handler = handle_http_client;
    start_http_server_listen(server_fd, &extra_args);

    now = time(NULL);
    ctime_r(&now, time_str);

    TRACE("Stopping server at %s", time_str);
    free_http_server();
}
