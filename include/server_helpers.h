#include "http.h"

int init_http_server(char *settings_json_path);
void free_http_server();

/* extra_arguments is passed to  */
int start_http_server_listen(int server_fd, HttpExtraArgs *extra_arguments);

HttpResponse return_http_error_code(HttpRequest request, int code, char *msg, Server_Settings settings);