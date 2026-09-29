#include "MyDb.h"
#include "factory.h"
#include "function.h"
#include "session.h"
#include "timer.h"

#include "../common/config.h"
#include "../common/connection.h"
#include "../common/crypto.h"
#include "../common/protocol.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace {

struct ServerOptions {
    std::string bindIp = "0.0.0.0";
    std::uint16_t port = 2000;
    int threadCount = 4;
    int idleTimeoutSeconds = 30;
    std::uint64_t maxFileSize = 10ULL * 1024ULL * 1024ULL * 1024ULL;
    std::string storageDir = "../workspace";
    std::string certificateFile = "../conf/server.crt";
    std::string privateKeyFile = "../conf/server.key";
    std::string dbHost = "127.0.0.1";
    std::string dbUser = "netdisk";
    std::string dbPassword = "netdisk_dev_password";
    std::string dbName = "Netdisk";
    unsigned int dbPort = 3306;
};

bool parseUInt16(const std::string &value, std::uint16_t &result)
{
    try {
        const unsigned long parsed = std::stoul(value);
        if (parsed > 65535UL) {
            return false;
        }
        result = static_cast<std::uint16_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

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

bool parseUInt64(const std::string &value, std::uint64_t &result)
{
    try {
        std::size_t consumed = 0;
        const unsigned long long parsed = std::stoull(value, &consumed, 10);
        if (consumed != value.size()) {
            return false;
        }
        result = static_cast<std::uint64_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool isHexString(const std::string &value, std::size_t expectedLength)
{
    if (value.size() != expectedLength) {
        return false;
    }
    for (char character : value) {
        const bool digit = character >= '0' && character <= '9';
        const bool lower = character >= 'a' && character <= 'f';
        const bool upper = character >= 'A' && character <= 'F';
        if (!digit && !lower && !upper) {
            return false;
        }
    }
    return true;
}

bool validName(const std::string &name)
{
    if (name.empty() || name == "." || name == ".." || name.size() > 255) {
        return false;
    }
    for (unsigned char character : name) {
        if (std::isspace(character) || character == '/' || character == '\\' ||
            character == '\0') {
            return false;
        }
    }
    return true;
}

class ServerApp {
public:
    ServerApp()
        : listenFd_(-1), epollFd_(-1), tlsContext_(nullptr), factory_(nullptr),
          timer_(nullptr)
    {
    }

    ~ServerApp()
    {
        if (timer_) {
            timer_->stop();
        }
        if (factory_) {
            factory_->stop();
        }
        {
            std::lock_guard<std::mutex> lock(connectionMutex_);
            connections_.clear();
        }
        if (tlsContext_ != nullptr) {
            SSL_CTX_free(tlsContext_);
        }
        if (epollFd_ >= 0) {
            close(epollFd_);
        }
        if (listenFd_ >= 0) {
            close(listenFd_);
        }
    }

    bool initialize(const std::string &configPath)
    {
        netdisk::KeyValueConfig config;
        std::string error;
        if (!config.load(configPath, error)) {
            std::cerr << error << std::endl;
            return false;
        }
        options_.bindIp = config.get("bind_ip", options_.bindIp);
        if (!parseUInt16(config.get("port", "2000"), options_.port)) {
            std::cerr << "invalid port in server config" << std::endl;
            return false;
        }
        options_.threadCount = config.getInt("thread_count", options_.threadCount);
        options_.idleTimeoutSeconds = config.getInt("idle_timeout_seconds",
                                                    options_.idleTimeoutSeconds);
        options_.maxFileSize = config.getUInt64("max_file_size_bytes",
                                                options_.maxFileSize);
        options_.storageDir = config.get("storage_dir", options_.storageDir);
        options_.certificateFile = config.get("tls_certificate", options_.certificateFile);
        options_.privateKeyFile = config.get("tls_private_key", options_.privateKeyFile);
        options_.dbHost = config.get("db_host", options_.dbHost);
        options_.dbUser = config.get("db_user", options_.dbUser);
        options_.dbPassword = config.get("db_password", options_.dbPassword);
        options_.dbName = config.get("db_name", options_.dbName);
        options_.dbPort = static_cast<unsigned int>(
            config.getInt("db_port", static_cast<int>(options_.dbPort)));
        if (options_.threadCount < 1 || options_.idleTimeoutSeconds < 1 ||
            options_.maxFileSize == 0 || options_.dbPort > 65535) {
            std::cerr << "invalid numeric server configuration" << std::endl;
            return false;
        }

        if (mkdir(options_.storageDir.c_str(), 0750) != 0 && errno != EEXIST) {
            std::cerr << "cannot create storage directory: " << options_.storageDir
                      << std::endl;
            return false;
        }
        if (!netdisk::initializeTls()) {
            std::cerr << "OpenSSL initialization failed" << std::endl;
            return false;
        }
        tlsContext_ = netdisk::createServerTlsContext(options_.certificateFile,
                                                      options_.privateKeyFile, error);
        if (tlsContext_ == nullptr) {
            std::cerr << error << std::endl;
            return false;
        }

        MyDb database;
        if (!database.initDB(options_.dbHost, options_.dbUser, options_.dbPassword,
                             options_.dbName, options_.dbPort, error)) {
            std::cerr << "database initialization failed: " << error << std::endl;
            return false;
        }

        listenFd_ = tcpInit(options_.bindIp, options_.port, error);
        if (listenFd_ < 0) {
            std::cerr << error << std::endl;
            return false;
        }
        epollFd_ = epoll_create1(EPOLL_CLOEXEC);
        if (epollFd_ < 0) {
            std::cerr << "epoll_create1 failed: " << std::strerror(errno) << std::endl;
            return false;
        }
        epoll_event event;
        std::memset(&event, 0, sizeof(event));
        event.events = EPOLLIN;
        event.data.fd = listenFd_;
        if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, listenFd_, &event) < 0) {
            std::cerr << "epoll_ctl failed: " << std::strerror(errno) << std::endl;
            return false;
        }

        factory_.reset(new Factory(options_.threadCount,
                                   [this](const Task &task) { handleTransfer(task); }));
        timer_.reset(new Timer(options_.idleTimeoutSeconds,
                               [this](int fd) { closeClient(fd, "idle timeout"); }));
        factory_->start();
        std::cout << "NetDisk server listening on " << options_.bindIp << ":"
                  << options_.port << std::endl;
        return true;
    }

    void run()
    {
        epoll_event events[64];
        while (true) {
            const int count = epoll_wait(epollFd_, events, 64, 1000);
            if (count < 0) {
                if (errno == EINTR) {
                    continue;
                }
                std::cerr << "epoll_wait failed: " << std::strerror(errno) << std::endl;
                return;
            }
            for (int i = 0; i < count; ++i) {
                const int fd = events[i].data.fd;
                if (fd == listenFd_) {
                    acceptClient();
                } else {
                    handleControl(fd);
                }
            }
        }
    }

private:
    bool connectDatabase(MyDb &database, std::string &error) const
    {
        return database.initDB(options_.dbHost, options_.dbUser, options_.dbPassword,
                               options_.dbName, options_.dbPort, error);
    }

    void acceptClient()
    {
        sockaddr_in peer;
        socklen_t length = sizeof(peer);
        const int fd = accept4(listenFd_, reinterpret_cast<sockaddr *>(&peer), &length,
                               SOCK_CLOEXEC);
        if (fd < 0) {
            std::cerr << "accept failed: " << std::strerror(errno) << std::endl;
            return;
        }

        const netdisk::ConnectionPtr connection =
            std::make_shared<netdisk::Connection>(fd);
        std::string error;
        if (!netdisk::acceptTls(connection, tlsContext_, error)) {
            std::cerr << error << std::endl;
            return;
        }

        std::string command;
        if (!netdisk::receiveText(*connection, command, error)) {
            return;
        }
        std::vector<std::string> words;
        if (!parseWords(command, words)) {
            netdisk::sendFrame(*connection, "ERR invalid command", error);
            return;
        }

        if (words[0] == "LOGIN" || words[0] == "REGISTER") {
            handleAuthentication(connection, words);
        } else if (words[0] == "PUT" || words[0] == "GET") {
            handleTransferRequest(connection, words);
        } else {
            netdisk::sendFrame(*connection, "ERR authentication required", error);
        }
    }

    void handleAuthentication(const netdisk::ConnectionPtr &connection,
                              std::vector<std::string> words)
    {
        MyDb database;
        std::string error;
        if (!connectDatabase(database, error)) {
            netdisk::sendFrame(*connection, "ERR database unavailable", error);
            return;
        }

        while (true) {
            if (words.size() != 3 || !validName(words[1]) || words[2].empty() ||
                words[2].size() > 256) {
                netdisk::sendFrame(*connection, "ERR invalid credentials", error);
            } else if (words[0] == "REGISTER") {
                bool exists = false;
                if (!database.userExists(words[1], exists, error)) {
                    netdisk::sendFrame(*connection, "ERR database error", error);
                } else if (exists) {
                    netdisk::sendFrame(*connection, "ERR user exists", error);
                } else {
                    std::string passwordHash;
                    if (!netdisk::hashPassword(words[2], passwordHash, error) ||
                        !database.createUser(words[1], passwordHash, error)) {
                        netdisk::sendFrame(*connection, "ERR registration failed", error);
                    } else {
                        database.writeLog(words[1], "REGISTER", error);
                        netdisk::sendFrame(*connection, "OK registered", error);
                    }
                }
            } else if (words[0] == "LOGIN") {
                std::string passwordHash;
                bool matched = false;
                if (!database.getPasswordHash(words[1], passwordHash, error) ||
                    passwordHash.empty() ||
                    !netdisk::verifyPassword(words[2], passwordHash, matched, error) ||
                    !matched) {
                    netdisk::sendFrame(*connection, "ERR invalid credentials", error);
                } else {
                    std::string token;
                    if (!sessions_.create(words[1], connection->fd, token, error)) {
                        netdisk::sendFrame(*connection, "ERR session failed", error);
                    } else if (!registerControlConnection(connection)) {
                        sessions_.removeFd(connection->fd);
                        netdisk::sendFrame(*connection, "ERR connection failed", error);
                        return;
                    } else {
                        database.writeLog(words[1], "LOGIN", error);
                        netdisk::sendFrame(*connection, "OK " + token, error);
                        return;
                    }
                }
            } else {
                netdisk::sendFrame(*connection, "ERR unsupported authentication command", error);
            }

            std::string nextCommand;
            if (!netdisk::receiveText(*connection, nextCommand, error) ||
                !parseWords(nextCommand, words)) {
                return;
            }
        }
    }

    bool registerControlConnection(const netdisk::ConnectionPtr &connection)
    {
        epoll_event event;
        std::memset(&event, 0, sizeof(event));
        event.events = EPOLLIN | EPOLLRDHUP;
        event.data.fd = connection->fd;
        if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, connection->fd, &event) < 0) {
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(connectionMutex_);
            connections_[connection->fd] = connection;
        }
        timer_->add(connection->fd);
        return true;
    }

    void handleControl(int fd)
    {
        netdisk::ConnectionPtr connection;
        {
            std::lock_guard<std::mutex> lock(connectionMutex_);
            const auto iterator = connections_.find(fd);
            if (iterator == connections_.end()) {
                return;
            }
            connection = iterator->second;
        }

        std::string command;
        std::string error;
        if (!netdisk::receiveText(*connection, command, error)) {
            closeClient(fd, "client disconnected");
            return;
        }

        std::vector<std::string> words;
        if (!parseWords(command, words)) {
            netdisk::sendFrame(*connection, "ERR invalid command", error);
            return;
        }
        if (words[0] == "QUIT") {
            netdisk::sendFrame(*connection, "OK bye", error);
            closeClient(fd, "client quit");
            return;
        }
        if (words.size() < 2) {
            netdisk::sendFrame(*connection, "ERR token required", error);
            return;
        }

        std::string user;
        if (!sessions_.validate(words[1], fd, user)) {
            netdisk::sendFrame(*connection, "ERR invalid session", error);
            closeClient(fd, "invalid session");
            return;
        }
        timer_->update(fd);

        MyDb database;
        if (!connectDatabase(database, error)) {
            netdisk::sendFrame(*connection, "ERR database unavailable", error);
            return;
        }

        std::uint64_t directoryId = 0;
        if (words[0] == "LS" && words.size() == 3 &&
            parseUInt64(words[2], directoryId)) {
            std::vector<FileEntry> entries;
            if (!database.listEntries(user, directoryId, entries, error)) {
                netdisk::sendFrame(*connection, "ERR database error", error);
                return;
            }
            std::ostringstream output;
            for (const FileEntry &entry : entries) {
                output << entry.name << '\t' << entry.type << '\t' << entry.size << '\n';
            }
            database.writeLog(user, "LS", error);
            netdisk::sendFrame(*connection, output.str(), error);
            return;
        }

        if ((words[0] == "MKDIR" || words[0] == "RMDIR" || words[0] == "RM") &&
            words.size() == 4 && parseUInt64(words[2], directoryId) &&
            validName(words[3])) {
            if (words[0] == "MKDIR") {
                const bool success = database.createDirectory(user, directoryId,
                                                               words[3], error);
                database.writeLog(user, "MKDIR " + words[3], error);
                const std::string response = success ? std::string("OK")
                                                     : std::string("ERR ") + error;
                netdisk::sendFrame(*connection, response, error);
                return;
            }
            if (words[0] == "RMDIR") {
                const bool success = database.deleteDirectory(user, directoryId,
                                                               words[3], error);
                database.writeLog(user, "RMDIR " + words[3], error);
                const std::string response = success ? std::string("OK")
                                                     : std::string("ERR ") + error;
                netdisk::sendFrame(*connection, response, error);
                return;
            }
            if (words[0] == "RM") {
                FileEntry deleted;
                const bool success = database.deleteFile(user, directoryId, words[3],
                                                         deleted, error);
                if (success) {
                    bool referenced = false;
                    database.hasOtherSha256Reference(deleted.sha256, user, directoryId,
                                                     words[3], referenced, error);
                    if (!referenced) {
                        const std::string path = options_.storageDir + "/" + deleted.sha256;
                        unlink(path.c_str());
                    }
                }
                database.writeLog(user, "RM " + words[3], error);
                const std::string response = success ? std::string("OK")
                                                     : std::string("ERR ") + error;
                netdisk::sendFrame(*connection, response, error);
                return;
            }
        }

        if (words[0] == "CD" && words.size() == 4 &&
            parseUInt64(words[2], directoryId)) {
            std::uint64_t nextDirectory = directoryId;
            if (words[3] == "..") {
                if (directoryId != 0 &&
                    !database.getParentId(user, directoryId, nextDirectory, error)) {
                    netdisk::sendFrame(*connection, "ERR invalid directory", error);
                    return;
                }
            } else {
                if (!validName(words[3])) {
                    netdisk::sendFrame(*connection, "ERR invalid directory name", error);
                    return;
                }
                FileEntry entry;
                if (!database.findEntry(user, directoryId, words[3], entry, error) ||
                    entry.type != 'd') {
                    netdisk::sendFrame(*connection, "ERR directory not found", error);
                    return;
                }
                nextDirectory = entry.id;
            }
            database.writeLog(user, "CD " + words[3], error);
            netdisk::sendFrame(*connection, "OK " + std::to_string(nextDirectory), error);
            return;
        }

        netdisk::sendFrame(*connection, "ERR invalid command arguments", error);
    }

    void handleTransferRequest(const netdisk::ConnectionPtr &connection,
                               const std::vector<std::string> &words)
    {
        std::string error;
        if ((words[0] != "PUT" && words[0] != "GET") || words.size() < 4) {
            netdisk::sendFrame(*connection, "ERR invalid transfer command", error);
            return;
        }

        std::string user;
        int authenticatedFd = -1;
        if (!sessions_.findUser(words[1], user, authenticatedFd) || !validName(words[3])) {
            netdisk::sendFrame(*connection, "ERR invalid session", error);
            return;
        }
        std::uint64_t directoryId = 0;
        if (!parseUInt64(words[2], directoryId)) {
            netdisk::sendFrame(*connection, "ERR invalid directory", error);
            return;
        }
        std::uint64_t offset = 0;
        if (words[0] == "GET" && (words.size() != 5 || !parseUInt64(words[4], offset))) {
            netdisk::sendFrame(*connection, "ERR invalid offset", error);
            return;
        }
        timer_->update(authenticatedFd);
        Task task;
        task.connection = connection;
        task.command = words[0];
        task.user = user;
        task.token = words[1];
        task.directoryId = directoryId;
        task.name = words[3];
        task.offset = offset;
        factory_->enqueue(task);
    }

    void handleTransfer(const Task &task)
    {
        MyDb database;
        std::string error;
        if (!connectDatabase(database, error)) {
            netdisk::sendFrame(*task.connection, "ERR database unavailable", error);
            return;
        }
        if (task.command == "PUT") {
            handleUpload(task, database);
        } else if (task.command == "GET") {
            handleDownload(task, database);
        }
    }

    void handleUpload(const Task &task, MyDb &database)
    {
        std::string error;
        FileEntry existing;
        if (database.findEntry(task.user, task.directoryId, task.name, existing, error)) {
            netdisk::sendFrame(*task.connection, "ERR name exists", error);
            return;
        }
        if (!error.empty()) {
            netdisk::sendFrame(*task.connection, "ERR database error", error);
            return;
        }

        std::string sha256;
        if (!netdisk::receiveText(*task.connection, sha256, error) ||
            !isHexString(sha256, 64)) {
            netdisk::sendFrame(*task.connection, "ERR invalid SHA-256", error);
            return;
        }
        const std::string finalPath = options_.storageDir + "/" + sha256;
        bool knownHash = false;
        if (!database.hasSha256(sha256, knownHash, error)) {
            netdisk::sendFrame(*task.connection, "ERR database error", error);
            return;
        }
        if (knownHash && access(finalPath.c_str(), F_OK) == 0) {
            struct stat existingFile;
            if (stat(finalPath.c_str(), &existingFile) != 0 || existingFile.st_size < 0) {
                netdisk::sendFrame(*task.connection, "ERR storage file missing", error);
                return;
            }
            if (!database.insertFile(task.user, task.directoryId, task.name,
                                     static_cast<std::uint64_t>(existingFile.st_size), sha256,
                                     error)) {
                netdisk::sendFrame(*task.connection, "ERR metadata insert failed", error);
                return;
            }
            database.writeLog(task.user, "PUT " + task.name + " (instant)", error);
            netdisk::sendFrame(*task.connection, "INSTANT", error);
            return;
        }

        if (!netdisk::sendFrame(*task.connection, "READY", error)) {
            return;
        }
        std::uint64_t fileSize = 0;
        if (!netdisk::receiveUInt64(*task.connection, fileSize, error) ||
            fileSize > options_.maxFileSize) {
            netdisk::sendFrame(*task.connection, "ERR invalid file size", error);
            return;
        }

        const std::string temporaryPath = options_.storageDir + "/.upload." + sha256 +
                                          "." + std::to_string(getpid()) + "." +
                                          std::to_string(task.connection->fd);
        std::ofstream output(temporaryPath.c_str(), std::ios::binary | std::ios::trunc);
        if (!output) {
            netdisk::sendFrame(*task.connection, "ERR cannot create storage file", error);
            return;
        }

        std::uint64_t received = 0;
        while (received < fileSize) {
            std::vector<unsigned char> chunk;
            if (!netdisk::receiveFrame(*task.connection, chunk, error,
                                       netdisk::kFileChunkSize) || chunk.empty() ||
                received + chunk.size() > fileSize) {
                output.close();
                unlink(temporaryPath.c_str());
                netdisk::sendFrame(*task.connection, "ERR incomplete upload", error);
                return;
            }
            output.write(reinterpret_cast<const char *>(chunk.data()), chunk.size());
            if (!output) {
                output.close();
                unlink(temporaryPath.c_str());
                netdisk::sendFrame(*task.connection, "ERR storage write failed", error);
                return;
            }
            received += chunk.size();
        }
        std::vector<unsigned char> endMarker;
        if (!netdisk::receiveFrame(*task.connection, endMarker, error,
                                   netdisk::kFileChunkSize) || !endMarker.empty()) {
            output.close();
            unlink(temporaryPath.c_str());
            netdisk::sendFrame(*task.connection, "ERR invalid upload terminator", error);
            return;
        }
        output.flush();
        output.close();

        std::string actualSha256;
        if (!netdisk::sha256File(temporaryPath, actualSha256, error) ||
            actualSha256 != sha256) {
            unlink(temporaryPath.c_str());
            netdisk::sendFrame(*task.connection, "ERR checksum mismatch", error);
            return;
        }

        if (link(temporaryPath.c_str(), finalPath.c_str()) != 0) {
            if (errno == EEXIST) {
                unlink(temporaryPath.c_str());
            } else {
                unlink(temporaryPath.c_str());
                netdisk::sendFrame(*task.connection, "ERR storage commit failed", error);
                return;
            }
        } else {
            unlink(temporaryPath.c_str());
        }

        if (!database.insertFile(task.user, task.directoryId, task.name, fileSize, sha256,
                                 error)) {
            netdisk::sendFrame(*task.connection, "ERR metadata insert failed", error);
            return;
        }
        database.writeLog(task.user, "PUT " + task.name, error);
        netdisk::sendFrame(*task.connection, "OK", error);
    }

    void handleDownload(const Task &task, MyDb &database)
    {
        std::string error;
        FileEntry file;
        if (!database.findEntry(task.user, task.directoryId, task.name, file, error) ||
            file.type != 'f') {
            netdisk::sendFrame(*task.connection, "NOT_FOUND", error);
            return;
        }
        if (task.offset > file.size) {
            netdisk::sendFrame(*task.connection, "ERR invalid offset", error);
            return;
        }
        const std::string path = options_.storageDir + "/" + file.sha256;
        std::ifstream input(path.c_str(), std::ios::binary);
        if (!input) {
            netdisk::sendFrame(*task.connection, "ERR storage file missing", error);
            return;
        }
        input.seekg(static_cast<std::streamoff>(task.offset));
        if (!netdisk::sendFrame(*task.connection, "FOUND " + file.sha256, error) ||
            !netdisk::sendUInt64(*task.connection, file.size, error)) {
            return;
        }

        std::vector<unsigned char> buffer(netdisk::kFileChunkSize);
        while (input) {
            input.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
            const std::streamsize count = input.gcount();
            if (count <= 0) {
                break;
            }
            if (!netdisk::sendFrame(*task.connection, buffer.data(),
                                    static_cast<std::uint32_t>(count), error)) {
                return;
            }
        }
        netdisk::sendFrame(*task.connection, nullptr, 0, error);
        database.writeLog(task.user, "GET " + task.name, error);
    }

    void closeClient(int fd, const std::string &reason)
    {
        epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr);
        timer_->remove(fd);
        sessions_.removeFd(fd);
        {
            std::lock_guard<std::mutex> lock(connectionMutex_);
            connections_.erase(fd);
        }
        std::cerr << "closed client fd=" << fd << ": " << reason << std::endl;
    }

    ServerOptions options_;
    int listenFd_;
    int epollFd_;
    SSL_CTX *tlsContext_;
    SessionStore sessions_;
    std::mutex connectionMutex_;
    std::unordered_map<int, netdisk::ConnectionPtr> connections_;
    std::unique_ptr<Factory> factory_;
    std::unique_ptr<Timer> timer_;
};

} // namespace

int main(int argc, char **argv)
{
    const std::string configPath = argc > 1 ? argv[1] : "../conf/server.conf";
    if (mysql_library_init(0, nullptr, nullptr) != 0) {
        std::cerr << "mysql_library_init failed" << std::endl;
        return 1;
    }

    ServerApp app;
    const bool initialized = app.initialize(configPath);
    if (initialized) {
        app.run();
    }
    mysql_library_end();
    return initialized ? 0 : 1;
}
