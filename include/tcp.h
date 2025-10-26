#pragma once

typedef struct tcp_args
{
    int client_fd;
    void *global_args;
}tcp_args;

/* returns fd for server */
int initialize_server(char *address, int port);

/* 
    listens for incomming connections and spawns a new thread which runs func
    arg for func is a tcp_args*
*/
int server_listen(int server_sockfd, int max_queued_req, void *(*func)(void*), void *global_args);

char *read_from_client(int client_fd, int max_read);
