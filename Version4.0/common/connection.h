#pragma once

#include <memory>
#include <string>

#include <openssl/ssl.h>

namespace netdisk {

struct Connection {
    explicit Connection(int socketFd = -1) : fd(socketFd), ssl(nullptr) {}
    ~Connection();

    Connection(const Connection &) = delete;
    Connection &operator=(const Connection &) = delete;

    int fd;
    SSL *ssl;
};

using ConnectionPtr = std::shared_ptr<Connection>;

bool initializeTls();
SSL_CTX *createServerTlsContext(const std::string &certificateFile,
                                const std::string &privateKeyFile,
                                std::string &error);
SSL_CTX *createClientTlsContext(std::string &error);
bool acceptTls(const ConnectionPtr &connection, SSL_CTX *context,
               std::string &error);
bool connectTls(const ConnectionPtr &connection, SSL_CTX *context,
                const std::string &host, std::string &error);

bool sendAll(const Connection &connection, const void *data, std::size_t length,
             std::string &error);
bool receiveAll(const Connection &connection, void *data, std::size_t length,
                std::string &error);
void closeConnection(ConnectionPtr &connection);

} // namespace netdisk
