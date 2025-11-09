#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "server_err.h"
#include "tcp.h"

int initialize_server(char *address, int port)
{
    int sockfd, opt = 1;
    struct sockaddr_in server_addr = { 0 };

    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1)
    {
        ERR("Could not create socket.\n");
        return -1;
    }
    TRACE("socket established FD=%d.\n", sockfd);

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        WARN("Could not set socket option SO_REUSEADDR(%d)\n", SO_REUSEADDR);

    server_addr.sin_family = AF_INET;
    inet_pton(server_addr.sin_family, address, &server_addr.sin_addr.s_addr);
    server_addr.sin_port = htons(port);

    TRACE("Binding to port=%d addr=%s...\n", port, address);
    if ((bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr))) != 0)
    {
        ERR("Bind failed. Ensure you have proper permissions to run on port %d.\n", port);
        return -1;
    }
    TRACE("Bind suceeded.\n");

    return sockfd;
}

int server_listen(int server_sockfd, int max_queued_req, void *(*func)(void*), void *global_args)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    TRACE("Server ready to listen...\n");
    if ((listen(server_sockfd, max_queued_req)) != 0)
    {
        ERR("Server failed to listen.\n");
        return 0;
    }
    TRACE("Server waiting for connections...\n");

    while (1)
    {
        tcp_args *args = calloc(1, sizeof(tcp_args));
        pthread_t thread;

        if (!args)
        {
            WARN("Error allocating space for tcp_args. server=%d\n", server_sockfd);
            continue;
        }

        args->client_fd = accept(server_sockfd, (struct sockaddr*)&client_addr, &client_len);
        if (args->client_fd < 0)
        {
            WARN("Error accepting client connection client_fd=%d\n", args->client_fd);
            free(args);
            continue;
        }
        TRACE("Client connection accepted, server_fd=%d client_fd=%d\n", server_sockfd, args->client_fd);
        args->global_args = global_args;

        if (pthread_create(&thread, NULL, func, (void*)args) != 0)
        {
            WARN("Could not create thread, server_fd=%d client_fd=%d\n", server_sockfd, args->client_fd);
            close(args->client_fd);
            free(args);
        } else
            pthread_detach(thread);
    }

    close(server_sockfd);
    return 1;
}

int server_listen_secure(int server_sockfd, int max_queued_req, void *(*func)(void*), void *global_args)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    SSL_CTX *ctx = create_context();
    configure_context(ctx);

    TRACE("Server ready to listen...\n");
    if ((listen(server_sockfd, max_queued_req)) != 0)
    {
        ERR("Server failed to listen.\n");
        return 0;
    }
    TRACE("Server waiting for connections...\n");

    while (1)
    {
        tcp_args *args = calloc(1, sizeof(tcp_args));
        pthread_t thread;

        if (!args)
        {
            WARN("Error allocating space for tcp_args. server=%d\n", server_sockfd);
            continue;
        }

        args->client_fd = accept(server_sockfd, (struct sockaddr*)&client_addr, &client_len);
        if (args->client_fd < 0)
        {
            WARN("Error accepting client connection client_fd=%d\n", args->client_fd);
            free(args);
            continue;
        }
        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, args->client_fd);
        
        if (SSL_accept(ssl) <= 0)
        {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            close(args->client_fd);
            free(args);
            ERR("Could not accept ssl client.\n");
            continue;
        }

        TRACE("Client connection accepted, server_fd=%d client_fd=%d\n", server_sockfd, args->client_fd);
        args->global_args = global_args;
        args->ssl = ssl;

        if (pthread_create(&thread, NULL, func, (void*)args) != 0)
        {
            WARN("Could not create thread, server_fd=%d client_fd=%d\n", server_sockfd, args->client_fd);
            close(args->client_fd);
            SSL_free(ssl);
            free(args);
        } else
            pthread_detach(thread);
    }

    close(server_sockfd);
    return 1;
}

int secure_recv(SSL *ssl, char *buffer, size_t buffer_length)
{
    return SSL_read(ssl, buffer, buffer_length);
}

int secure_send(SSL *ssl, char *response, size_t count)
{
    return SSL_write(ssl, response, count);
}
