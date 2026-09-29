#include "connection.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace netdisk {

namespace {

void setSocketTimeouts(int fd)
{
    timeval timeout;
    timeout.tv_sec = 30;
    timeout.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}

bool setError(std::string &error, const std::string &message)
{
    error = message;
    return false;
}

} // namespace

Connection::~Connection()
{
    if (ssl != nullptr) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = nullptr;
    }
    if (fd >= 0) {
        close(fd);
        fd = -1;
    }
}

bool initializeTls()
{
    return OPENSSL_init_ssl(0, nullptr) == 1;
}

SSL_CTX *createServerTlsContext(const std::string &certificateFile,
                                const std::string &privateKeyFile,
                                std::string &error)
{
    SSL_CTX *context = SSL_CTX_new(TLS_server_method());
    if (context == nullptr) {
        return nullptr;
    }
    SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION);
    if (SSL_CTX_use_certificate_file(context, certificateFile.c_str(),
                                     SSL_FILETYPE_PEM) != 1) {
        error = "cannot load TLS certificate: " + certificateFile;
        SSL_CTX_free(context);
        return nullptr;
    }
    if (SSL_CTX_use_PrivateKey_file(context, privateKeyFile.c_str(),
                                    SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_check_private_key(context) != 1) {
        error = "cannot load or validate TLS private key: " + privateKeyFile;
        SSL_CTX_free(context);
        return nullptr;
    }
    return context;
}

SSL_CTX *createClientTlsContext(std::string &error)
{
    SSL_CTX *context = SSL_CTX_new(TLS_client_method());
    if (context == nullptr) {
        error = "cannot create TLS client context";
        return nullptr;
    }
    SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION);

    // Local development uses a self-signed certificate. Production deployments
    // should load a CA and enable certificate verification.
    SSL_CTX_set_verify(context, SSL_VERIFY_NONE, nullptr);
    return context;
}

bool acceptTls(const ConnectionPtr &connection, SSL_CTX *context,
               std::string &error)
{
    if (!connection || connection->fd < 0 || context == nullptr) {
        return setError(error, "invalid connection or TLS context");
    }
    setSocketTimeouts(connection->fd);
    connection->ssl = SSL_new(context);
    if (connection->ssl == nullptr) {
        return setError(error, "SSL_new failed");
    }
    SSL_set_fd(connection->ssl, connection->fd);
    if (SSL_accept(connection->ssl) != 1) {
        SSL_free(connection->ssl);
        connection->ssl = nullptr;
        return setError(error, "TLS handshake failed");
    }
    return true;
}

bool connectTls(const ConnectionPtr &connection, SSL_CTX *context,
                const std::string &host, std::string &error)
{
    if (!connection || connection->fd < 0 || context == nullptr) {
        return setError(error, "invalid connection or TLS context");
    }
    setSocketTimeouts(connection->fd);
    connection->ssl = SSL_new(context);
    if (connection->ssl == nullptr) {
        return setError(error, "SSL_new failed");
    }
    SSL_set_tlsext_host_name(connection->ssl, host.c_str());
    SSL_set_fd(connection->ssl, connection->fd);
    if (SSL_connect(connection->ssl) != 1) {
        SSL_free(connection->ssl);
        connection->ssl = nullptr;
        return setError(error, "TLS handshake failed");
    }
    return true;
}

bool sendAll(const Connection &connection, const void *data, std::size_t length,
             std::string &error)
{
    const char *current = static_cast<const char *>(data);
    std::size_t sent = 0;
    while (sent < length) {
        const int result = connection.ssl == nullptr
                               ? static_cast<int>(send(connection.fd, current + sent,
                                                       length - sent, MSG_NOSIGNAL))
                               : SSL_write(connection.ssl, current + sent,
                                           static_cast<int>(length - sent));
        if (result <= 0) {
            if (errno == EINTR) {
                continue;
            }
            return setError(error, "send failed: " + std::string(std::strerror(errno)));
        }
        sent += static_cast<std::size_t>(result);
    }
    return true;
}

bool receiveAll(const Connection &connection, void *data, std::size_t length,
                std::string &error)
{
    char *current = static_cast<char *>(data);
    std::size_t received = 0;
    while (received < length) {
        const int result = connection.ssl == nullptr
                               ? static_cast<int>(recv(connection.fd, current + received,
                                                       length - received, 0))
                               : SSL_read(connection.ssl, current + received,
                                          static_cast<int>(length - received));
        if (result == 0) {
            return setError(error, "peer closed the connection");
        }
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return setError(error, "receive failed: " + std::string(std::strerror(errno)));
        }
        received += static_cast<std::size_t>(result);
    }
    return true;
}

void closeConnection(ConnectionPtr &connection)
{
    if (connection) {
        connection.reset();
    }
}

} // namespace netdisk
