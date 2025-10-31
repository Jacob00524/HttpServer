#include <stdio.h>
#include <string.h>
#include "server_err.h"
#include "config_handler.h"
#include "http.h"
#include "server_helpers.h"

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
    Server_Settings settings;

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
    {
        config_free(&settings);
        return -1;
    }

    return server_fd;
}

void free_http_server()
{
    Server_Settings settings;

    settings = get_server_settings();
    config_free(&settings);
}

int start_http_server_listen(int server_fd, HttpExtraArgs *extra_arguments)
{
    Server_Settings settings;

    settings = get_server_settings();
    if (!server_listen(server_fd, settings.max_queue, http_routine, (void*)extra_arguments))
    {
        ERR("An error occurred during http server operation.\n");
        return 0;
    }
    return 1;
}