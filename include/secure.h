#include <openssl/ssl.h>
#include <openssl/err.h>

SSL_CTX *create_context();
void configure_context(SSL_CTX *ctx);
