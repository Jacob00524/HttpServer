#pragma once
#include "secure.h"

typedef struct tcp_args
{
    int client_fd;
    SSL *ssl;
    void *global_args;
}tcp_args;

/* returns fd for server */
int initialize_server(char *address, int port);
void stop_server();

/* 
    listens for incomming connections and spawns a new thread which runs func
    arg for func is a tcp_args*
*/
int server_listen(int server_sockfd, int max_queued_req, void *(*func)(void*), void *global_args);
int server_listen_secure(int server_sockfd, int max_queued_req, void *(*func)(void*), void *global_args);

int secure_recv(SSL *ssl, char *buffer, size_t buffer_length);
int secure_send(SSL *ssl, char *response, size_t count);
