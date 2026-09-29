#include "function.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int tcpInit(const std::string &bindIp, std::uint16_t port, std::string &error)
{
    const int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd < 0) {
        error = "socket failed: " + std::string(std::strerror(errno));
        return -1;
    }

    int enabled = 1;
    setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));

    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, bindIp.c_str(), &address.sin_addr) != 1) {
        error = "invalid bind address: " + bindIp;
        close(socketFd);
        return -1;
    }
    if (bind(socketFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        error = "bind failed: " + std::string(std::strerror(errno));
        close(socketFd);
        return -1;
    }
    if (listen(socketFd, 128) < 0) {
        error = "listen failed: " + std::string(std::strerror(errno));
        close(socketFd);
        return -1;
    }
    return socketFd;
}
