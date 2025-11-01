#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include "server_err.h"
#include "config_handler.h"
#include "http.h"
#include "server_helpers.h"

static HttpResponse default_client_handler(HttpRequest *request, HttpExtraArgs *extra_args);

static int create_server_structure(Server_Settings settings)
{
    FILE *index_f;
    char index_path[strlen(settings.content_folder) + strlen(settings.index_name) + 2];
    int result;

    char sample_index[] = {"\
<html>\n\
    <header>\n\
        <title>Index</title>\n\
    </header>\n\
    <body>\n\
        <h1>Hello World!</h1>\n\
        <p>This is a sample index page.</p>\n\
    </body>\n\
</html>"};
    char sample_error[] = {"\
<html>\n\
    <header>\n\
        <title>Error 404</title>\n\
    </header>\n\
    <body>\n\
        <h1>Error 404 Not Found</h1>\n\
        <p>An error occured. But, please don't panic, this is a generic message and can happen for many reasons.</p>\n\
    </body>\n\
</html>"};

    if (!create_folder(settings.content_folder))
        return 0;
    if (!create_folder(settings.error_folder))
        return 0;
    if (!create_file(sample_index, settings.content_folder, settings.index_name))
        return 0;
    if (!create_file(sample_error, settings.error_folder, "404.html"))
        return 0;
    return 1;
}

int init_http_server(char *settings_json_path)
{
    int config_result, server_fd;
    Server_Settings settings = { 0 };

    /* looks for a settings file, if not found create it. */
    if ((config_result = read_config(settings_json_path, &settings)) == 0)
    {
        if (!create_config(settings_json_path, &settings))
        {
            ERR("Could not create server settings config file.\n");
            return -1;
        }
    }else if (config_result == -1)
    {
        ERR("Could not read config file.\n");
        return -1;
    }
    set_server_settings(settings);

    /* Makes sure server structure is setup, creates it if its not. */
    if (!create_server_structure(settings))
    {
        ERR("An error occured while trying to setup the server.\n");
        return -1;
    }

    server_fd = initialize_server(settings.address, settings.port);
    if (server_fd == -1)
        return -1;

    return server_fd;
}

void free_http_server()
{
    /* does nothing right now. */
}

int start_http_server_listen(int server_fd, HttpExtraArgs *extra_arguments)
{
    Server_Settings settings;

    settings = get_server_settings();
    if (extra_arguments->client_handler == NULL)
        extra_arguments->client_handler = default_client_handler;
    if (!server_listen(server_fd, settings.max_queue, http_routine, (void*)extra_arguments))
    {
        ERR("An error occurred during http server operation.\n");
        return 0;
    }
    return 1;
}

HttpResponse return_http_error_code(HttpRequest request, int code, char *msg, Server_Settings settings)
{
    char path[1024], file_data[1024], header_data[1024];
    size_t file_length;
    FILE *error_file;
    HttpResponse response;

    response.return_code = code;
    strcpy(response.connection, "close");
    strcpy(response.msg_code, msg);
    strcpy(response.content_type, "text/html");
    file_data[0] = 0;

    if (settings.error_folder)
    {
        sprintf(path, "%s/%d.html", settings.error_folder, code);
        error_file = fopen(path, "r");
        if (error_file)
        {
            fseek(error_file, 0, SEEK_END);
            file_length = ftell(error_file);
            fseek(error_file, 0, SEEK_SET);

            if (file_length < sizeof(file_data))
            {
                fread(file_data, sizeof(char), file_length, error_file);
                file_data[file_length] = 0;
            }else
                WARN("error file length is too large.\n");
            fclose(error_file);
        }else
            WARN("Could not find %s, defaulting to built-in response.\n", path);
    }
    if (file_data[0] == 0)
    {
        sprintf(file_data, "\
<html>\n\
    <header>\n\
        <title>Error %d</title>\n\
    </header>\n\
    <body>\n\
        <h1>Error %d %s</h1>\n\
        <p>An error occured. But, please don't panic, this is a generic message and can happen for many reasons.</p>\n\
    </body>\n\
</html>", code, code, msg);
    }

    response.content_length = strlen(file_data);

    if (craft_basic_headers(response, header_data, sizeof(header_data)) == sizeof(header_data)-1)
        WARN("It is possilbe not all header data was written.\n");
    
    send(request.connection_info.client_fd, header_data, strlen(header_data), 0);
    send(request.connection_info.client_fd, file_data, response.content_length, 0);

    return response;
}

HttpResponse handle_default_HTTP_GET(HttpRequest *request)
{
    HttpResponse response = { 0 };
    Server_Settings settings = get_server_settings();
    FILE *requested_file;
    char *file_data;
    size_t file_length;
    char header_data[1024], path[1024];

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
}

HttpResponse handle_default_HTTP_POST(HttpRequest *request, Server_Settings settings)
{
    return return_http_error_code(*request, 404, "Not Found", settings);
}

static HttpResponse default_client_handler(HttpRequest *request, HttpExtraArgs *extra_args)
{
    HttpResponse response;
    Server_Settings settings;

    settings = get_server_settings();

    if (!strcmp(request->method, "GET"))
    {
        if (!extra_args->GET_handler)
            return handle_default_HTTP_GET(request);
        else
            return extra_args->GET_handler(request);
    }
    else if (!strcmp(request->method, "POST"))
    {
        if (!extra_args->POST_handler)
            return handle_default_HTTP_POST(request, settings);
        else
            return extra_args->POST_handler(request);
    }
    else
    {
        WARN("Unhandled http method.\n");
    }
    return response;
}

int add_header(char *header_buffer, size_t max_size, char *new_headers)
{
    char *end_last_header;

    if (!header_buffer || !new_headers || max_size <= (strlen(header_buffer) + strlen(new_headers)))
        return 0;

    end_last_header = strstr(header_buffer, "\r\n\r\n");
    if (!end_last_header)
        return 0;
    end_last_header += 2;

    strcpy(end_last_header, new_headers);
    end_last_header = strstr(header_buffer, "\r\n\r\n");
    if (end_last_header)
        return 1;

    if (header_buffer[strlen(header_buffer) - 1] == '\n')
    {
        strcat(header_buffer, "\r\n");
        return 1;
    }

    if (max_size <= (strlen(header_buffer) + strlen(new_headers) + 2))
        return 0;
    strcat(header_buffer, "\r\n\r\n");
    return 1;
}

HttpResponse send_http_redirect(HttpRequest* request, char *location, char *addition_headers, Server_Settings settings)
{
    char header_data[1024];
    char new_loc_header[256];
    HttpResponse response = { 0 };

    strcpy(response.connection, "close");
    strcpy(response.msg_code, "See Other");
    response.return_code = 303;

    if (craft_basic_headers(response, header_data, sizeof(header_data)) == sizeof(header_data)-1)
        WARN("It is possilbe not all header data was written.\n");
    sprintf(new_loc_header, "Location: %s\r\n", location);
    if (!add_header(header_data, sizeof(header_data), new_loc_header))
        return return_http_error_code(*request, 400, "Bad Request", settings);
    if (!add_header(header_data, sizeof(header_data), addition_headers))
        return return_http_error_code(*request, 400, "Bad Request", settings);

    send(request->connection_info.client_fd, header_data, strlen(header_data), 0);
    return response;
}
