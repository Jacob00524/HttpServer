#include <time.h>
#include "server_err.h"
#include "server_helpers.h"

char *server_settings_path = "server_settings.json";

int main()
{
    int server_fd;
    time_t now;
    char time_str[26];
    HttpExtraArgs extra_args = { 0 };

    now = time(NULL);
    ctime_r(&now, time_str);

    TRACE("Starting server at %s", time_str);
    server_fd = init_http_server(server_settings_path);

    start_http_server_listen(server_fd, &extra_args);

    now = time(NULL);
    ctime_r(&now, time_str);

    TRACE("Stopping server at %s", time_str);
    free_http_server();
}
