
#include <openssl/ssl.h>
#include <openssl/err.h>

typedef struct tcp_args
{
    int client_fd;
    SSL *ssl;
    void *global_args;
}tcp_args;

void tcp_server_cleanup();
void stop_tcp_server();
int initialize_server(const char *address, int port);
void tcp_args_destroy(tcp_args *a);

int server_listen(int server_sockfd, int max_queued_req, void *(*func)(void*), void *global_args);
int server_listen_secure(int server_sockfd, int max_queued_req, void *(*func)(void*), void *global_args);
int secure_recv(SSL *ssl, char *buffer, size_t buffer_length);
int secure_send(SSL *ssl, char *response, size_t count);
