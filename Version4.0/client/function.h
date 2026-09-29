#pragma once

#include "../common/config.h"
#include "../common/connection.h"

#include <cstdint>
#include <string>

struct ClientOptions {
    std::string serverIp = "127.0.0.1";
    std::uint16_t port = 2000;
    std::uint64_t maxFileSize = 10ULL * 1024ULL * 1024ULL * 1024ULL;
};

struct ClientContext {
    ClientOptions options;
    SSL_CTX *tlsContext = nullptr;
    std::string username;
    std::string token;
    std::uint64_t directoryId = 0;
};

netdisk::ConnectionPtr connectToServer(const ClientContext &context,
                                       std::string &error);
bool uploadFile(const ClientContext &context, const std::string &localPath);
bool downloadFile(const ClientContext &context, const std::string &fileName);
