#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

class SessionStore {
public:
    bool create(const std::string &user, int fd, std::string &token,
                std::string &error);
    bool validate(const std::string &token, int fd, std::string &user) const;
    bool findUser(const std::string &token, std::string &user, int &fd) const;
    void removeFd(int fd);

private:
    struct Session {
        std::string user;
        int fd = -1;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Session> sessions_;
    std::unordered_map<int, std::string> fdToToken_;
};
