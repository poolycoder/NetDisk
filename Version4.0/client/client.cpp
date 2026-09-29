#include "function.h"

#include "../common/connection.h"
#include "../common/protocol.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {

bool parseWords(const std::string &command, std::vector<std::string> &words)
{
    words.clear();
    std::istringstream stream(command);
    std::string word;
    while (stream >> word) {
        words.push_back(word);
    }
    return !words.empty();
}

bool validName(const std::string &name)
{
    if (name.empty() || name == "." || name == ".." || name.size() > 255) {
        return false;
    }
    for (unsigned char character : name) {
        if (std::isspace(character) || character == '/' || character == '\\') {
            return false;
        }
    }
    return true;
}

std::string baseName(const std::string &path)
{
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

bool loadOptions(ClientOptions &options, const std::string &path)
{
    netdisk::KeyValueConfig config;
    std::string error;
    if (!config.load(path, error)) {
        std::cerr << error << std::endl;
        return false;
    }
    options.serverIp = config.get("server_ip", options.serverIp);
    try {
        const unsigned long port = std::stoul(config.get("server_port", "2000"));
        if (port > 65535UL) {
            throw std::runtime_error("port out of range");
        }
        options.port = static_cast<std::uint16_t>(port);
    } catch (...) {
        std::cerr << "invalid server_port" << std::endl;
        return false;
    }
    options.maxFileSize = config.getUInt64("max_file_size_bytes", options.maxFileSize);
    return true;
}

} // namespace

netdisk::ConnectionPtr connectToServer(const ClientContext &context,
                                       std::string &error)
{
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        error = "socket failed: " + std::string(std::strerror(errno));
        return nullptr;
    }
    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(context.options.port);
    if (inet_pton(AF_INET, context.options.serverIp.c_str(), &address.sin_addr) != 1) {
        error = "invalid server address";
        close(fd);
        return nullptr;
    }
    if (connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        error = "connect failed: " + std::string(std::strerror(errno));
        close(fd);
        return nullptr;
    }
    const netdisk::ConnectionPtr connection = std::make_shared<netdisk::Connection>(fd);
    if (!netdisk::connectTls(connection, context.tlsContext, context.options.serverIp,
                             error)) {
        return nullptr;
    }
    return connection;
}

int main()
{
    ClientOptions options;
    if (!loadOptions(options, "../conf/client.conf")) {
        return 1;
    }
    if (!netdisk::initializeTls()) {
        std::cerr << "OpenSSL initialization failed" << std::endl;
        return 1;
    }
    std::string error;
    SSL_CTX *tlsContext = netdisk::createClientTlsContext(error);
    if (tlsContext == nullptr) {
        std::cerr << error << std::endl;
        return 1;
    }

    ClientContext context;
    context.options = options;
    context.tlsContext = tlsContext;
    netdisk::ConnectionPtr connection = connectToServer(context, error);
    if (!connection) {
        std::cerr << error << std::endl;
        SSL_CTX_free(tlsContext);
        return 1;
    }
    std::cout << "connected to " << options.serverIp << ":" << options.port << std::endl;

    while (context.token.empty()) {
        std::cout << "1. login  2. register  3. quit\nchoice: ";
        std::string choice;
        std::getline(std::cin, choice);
        if (choice == "3" || !std::cin) {
            break;
        }
        if (choice != "1" && choice != "2") {
            std::cout << "invalid choice\n";
            continue;
        }
        std::string username;
        std::string password;
        std::cout << "username: ";
        std::getline(std::cin, username);
        std::cout << "password: ";
        std::getline(std::cin, password);
        const std::string operation = (choice == "1" ? "LOGIN " : "REGISTER ") +
                                      username + " " + password;
        if (!netdisk::sendFrame(*connection, operation, error)) {
            std::cerr << error << std::endl;
            break;
        }
        std::string response;
        if (!netdisk::receiveText(*connection, response, error)) {
            std::cerr << error << std::endl;
            break;
        }
        if (choice == "2") {
            std::cout << response << std::endl;
            continue;
        }
        if (response.compare(0, 3, "OK ") != 0) {
            std::cout << response << std::endl;
            continue;
        }
        context.username = username;
        context.token = response.substr(3);
        std::cout << "login succeeded\n";
    }

    while (!context.token.empty() && std::cin) {
        std::cout << context.username << "@client:";
        std::string line;
        std::getline(std::cin, line);
        std::vector<std::string> words;
        if (!parseWords(line, words)) {
            continue;
        }
        const std::string command = words[0];
        if (command == "quit") {
            netdisk::sendFrame(*connection, "QUIT " + context.token, error);
            break;
        }
        if (command == "puts" && words.size() == 2) {
            if (!uploadFile(context, words[1])) {
                std::cout << "upload failed\n";
            }
            continue;
        }
        if (command == "gets" && words.size() == 2 && validName(words[1])) {
            if (!downloadFile(context, words[1])) {
                std::cout << "download failed\n";
            }
            continue;
        }

        std::string request;
        if (command == "ls" && words.size() == 1) {
            request = "LS " + context.token + " " + std::to_string(context.directoryId);
        } else if (command == "mkdir" && words.size() == 2 && validName(words[1])) {
            request = "MKDIR " + context.token + " " +
                      std::to_string(context.directoryId) + " " + words[1];
        } else if (command == "rmdir" && words.size() == 2 && validName(words[1])) {
            request = "RMDIR " + context.token + " " +
                      std::to_string(context.directoryId) + " " + words[1];
        } else if (command == "rm" && words.size() == 2 && validName(words[1])) {
            request = "RM " + context.token + " " +
                      std::to_string(context.directoryId) + " " + words[1];
        } else if (command == "cd" && words.size() == 2 &&
                   (words[1] == ".." || validName(words[1]))) {
            request = "CD " + context.token + " " +
                      std::to_string(context.directoryId) + " " + words[1];
        } else {
            std::cout << "commands: ls, mkdir NAME, rmdir NAME, cd NAME, rm NAME, "
                         "puts PATH, gets NAME, quit\n";
            continue;
        }

        if (!netdisk::sendFrame(*connection, request, error)) {
            std::cerr << error << std::endl;
            break;
        }
        std::string response;
        if (!netdisk::receiveText(*connection, response, error)) {
            std::cerr << error << std::endl;
            break;
        }
        if (command == "cd" && response.compare(0, 3, "OK ") == 0) {
            context.directoryId = std::stoull(response.substr(3));
            std::cout << "directory changed\n";
        } else {
            std::cout << response;
            if (response.empty() || response.back() != '\n') {
                std::cout << '\n';
            }
        }
    }

    connection.reset();
    SSL_CTX_free(tlsContext);
    return 0;
}
