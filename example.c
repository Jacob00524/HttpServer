#include <signal.h>
#include <time.h>

#include "server_err.h"
#include "server_helpers.h"

char *server_settings_path = "server_settings.json";

static void on_sigint(int sig)
{
    (void)sig;
    stop_server();
}

void setup_signal_handlers()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigint;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
}

int main()
{
    int server_fd;
    time_t now;
    char time_str[26];
    HttpExtraArgs extra_args = { 0 };

    setup_signal_handlers();

    now = time(NULL);
    ctime_r(&now, time_str);

    TRACE("Starting server at %s", time_str);
    server_fd = init_http_server(server_settings_path);

    start_http_server_listen(server_fd, &extra_args, 1); /* set secure to 1 for https and 0 for http */

    now = time(NULL);
    ctime_r(&now, time_str);

    TRACE("Stopping server at %s", time_str);
    free_http_server();
}
