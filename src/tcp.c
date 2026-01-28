#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <sys/eventfd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "http.h"

static volatile sig_atomic_t server_running = 1;
static int g_listen_fd = -1;
static int g_shutdown_efd = -1;

static int server_shutdown_init()
{
    if (g_shutdown_efd != -1)
        return 1;

    g_shutdown_efd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    return (g_shutdown_efd != -1);
}

void tcp_server_cleanup()
{
    if (g_shutdown_efd != -1)
    {
        close(g_shutdown_efd);
        g_shutdown_efd = -1;
    }
}

void stop_tcp_server()
{
    server_running = 0;

    if (g_shutdown_efd != -1)
    {
        uint64_t one = 1;
        (void)write(g_shutdown_efd, &one, sizeof(one));
    }
}

void tcp_args_destroy(tcp_args *a)
{
    if (!a) return;

    if (a->ssl)
    {
        SSL_shutdown(a->ssl);
        SSL_free(a->ssl);
        a->ssl = NULL;
    }

    if (a->client_fd != -1)
    {
        close(a->client_fd);
        a->client_fd = -1;
    }
}

SSL_CTX *create_context()
{
    const SSL_METHOD *method = TLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);

    if (!ctx)
    {
        ERR_print_errors_fp(stderr);
        exit(1);
    }
    return ctx;
}

int configure_context(SSL_CTX *ctx)
{
    if (SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) <= 0 || SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        return 0;
    }
    return 1;
}

/*
    Returns -1 on failure, if address is NULL then INADDR_ANY is set
*/
int initialize_server(const char *address, int port)
{
    int sockfd, opt = 1;
    struct sockaddr_in server_addr = { 0 };

    if (port < 1 || port > 65535)
    {
        ERR("Invalid port: %d\n", port);
        return -1;
    }

    if (!server_shutdown_init())
    {
        ERR("server shutdown init failed: %s\n", strerror(errno));
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (!address || address[0] == '\0')
    {
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        address = "0.0.0.0";
    }else
    {
        int address_result = inet_pton(AF_INET, address, &server_addr.sin_addr);
        if (address_result != 1)
        {
            if (address_result == 0)
                ERR("Invalid IPv4 address string: '%s'\n", address);
            else
                ERR("inet_pton failed for '%s': %s\n", address, strerror(errno));
            return -1;
        }
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1)
    {
        ERR("Could not create socket.\n");
        return -1;
    }
    TRACE("socket established FD=%d.\n", sockfd);

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        WARN("Could not set SO_REUSEADDR: %s\n", strerror(errno));

    TRACE("Binding to port=%d addr=%s...\n", port, address);
    if ((bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr))) != 0)
    {
        ERR("Bind failed on %s:%d: %s\n", address, port, strerror(errno));
        close(sockfd);
        return -1;
    }

    TRACE("Bind suceeded.\n");
    return sockfd;
}

int server_listen(int server_sockfd, int max_queued_req, void *(*func)(void*), void *global_args)
{
    struct pollfd fds[2];

    if (g_shutdown_efd == -1)
    {
        ERR("server_shutdown_init() was not called.\n");
        return 0;
    }

    TRACE("Server ready to listen...\n");
    if (listen(server_sockfd, max_queued_req) != 0)
    {
        ERR("Server failed to listen: %s\n", strerror(errno));
        return 0;
    }

    g_listen_fd = server_sockfd;
    TRACE("Server waiting for connections...\n");

    memset(fds, 0, sizeof(fds));
    fds[0].fd = server_sockfd;
    fds[0].events = POLLIN;
    fds[1].fd = g_shutdown_efd;
    fds[1].events = POLLIN;

    while (server_running)
    {
        int prc = poll(fds, 2, -1);
        if (prc < 0)
        {
            if (errno == EINTR) continue;
            ERR("poll failed: %s\n", strerror(errno));
            break;
        }

        if (fds[1].revents & POLLIN)
        {
            uint64_t tmp;
            (void)read(g_shutdown_efd, &tmp, sizeof(tmp));
            break;
        }

        if (fds[0].revents & POLLIN)
        {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            pthread_t thread;

            int cfd = accept(server_sockfd, (struct sockaddr*)&client_addr, &client_len);
            if (cfd < 0)
            {
                if (!server_running) break;
                if (errno == EINTR) continue;
                if (errno == EBADF || errno == EINVAL) break;

                WARN("accept failed: %s\n", strerror(errno));
                continue;
            }

            tcp_args *args = calloc(1, sizeof(*args));
            if (!args)
            {
                WARN("calloc tcp_args failed\n");
                close(cfd);
                continue;
            }

            args->client_fd = cfd;
            args->global_args = global_args;

            if (pthread_create(&thread, NULL, func, args) != 0)
            {
                WARN("pthread_create failed, client_fd=%d\n", cfd);
                close(cfd);
                free(args);
                continue;
            }

            pthread_detach(thread);
        }
    }

    if (g_listen_fd == server_sockfd)
        g_listen_fd = -1;

    close(server_sockfd);
    return 1;
}

int server_listen_secure(int server_sockfd, int max_queued_req, void *(*func)(void*), void *global_args)
{
    if (g_shutdown_efd == -1)
    {
        ERR("server_shutdown_init() was not called.\n");
        return 0;
    }

    SSL_CTX *ctx = create_context();
    if (!ctx)
    {
        ERR("create_context failed.\n");
        return 0;
    }

    if (!configure_context(ctx))
    {
        ERR("configure_context failed.\n");
        SSL_CTX_free(ctx);
        return 0;
    }

    TRACE("Secure server ready to listen...\n");
    if (listen(server_sockfd, max_queued_req) != 0)
    {
        ERR("Server failed to listen: %s\n", strerror(errno));
        SSL_CTX_free(ctx);
        return 0;
    }

    g_listen_fd = server_sockfd;
    TRACE("Secure server waiting for connections...\n");

    struct pollfd fds[2];
    memset(fds, 0, sizeof(fds));
    fds[0].fd = server_sockfd;
    fds[0].events = POLLIN;
    fds[1].fd = g_shutdown_efd;
    fds[1].events = POLLIN;

    while (server_running)
    {
        int prc = poll(fds, 2, -1);
        if (prc < 0)
        {
            if (errno == EINTR) continue;
            ERR("poll failed: %s\n", strerror(errno));
            break;
        }

        if (fds[1].revents & POLLIN)
        {
            uint64_t tmp;
            (void)read(g_shutdown_efd, &tmp, sizeof(tmp));
            break;
        }

        if (fds[0].revents & POLLIN)
        {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            pthread_t thread;

            int cfd = accept(server_sockfd, (struct sockaddr*)&client_addr, &client_len);
            if (cfd < 0)
            {
                if (!server_running) break;
                if (errno == EINTR) continue;
                if (errno == EBADF || errno == EINVAL) break;

                WARN("accept failed: %s\n", strerror(errno));
                continue;
            }

            SSL *ssl = SSL_new(ctx);
            if (!ssl)
            {
                ERR_print_errors_fp(stderr);
                close(cfd);
                continue;
            }

            if (SSL_set_fd(ssl, cfd) != 1)
            {
                ERR_print_errors_fp(stderr);
                SSL_free(ssl);
                close(cfd);
                continue;
            }

            if (SSL_accept(ssl) <= 0)
            {
                ERR_print_errors_fp(stderr);
                SSL_free(ssl);
                close(cfd);
                continue;
            }

            tcp_args *args = calloc(1, sizeof(*args));
            if (!args)
            {
                WARN("calloc tcp_args failed\n");
                SSL_shutdown(ssl);
                SSL_free(ssl);
                close(cfd);
                continue;
            }

            args->client_fd = cfd;
            args->ssl = ssl;
            args->global_args = global_args;

            if (pthread_create(&thread, NULL, func, args) != 0)
            {
                WARN("pthread_create failed, client_fd=%d\n", cfd);
                SSL_shutdown(ssl);
                SSL_free(ssl);
                close(cfd);
                free(args);
                continue;
            }

            pthread_detach(thread);
        }
    }

    if (g_listen_fd == server_sockfd)
        g_listen_fd = -1;

    close(server_sockfd);
    SSL_CTX_free(ctx);
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
