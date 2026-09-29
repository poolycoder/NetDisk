#include "session.h"

#include "../common/crypto.h"

bool SessionStore::create(const std::string &user, int fd, std::string &token,
                          std::string &error)
{
    if (!netdisk::randomToken(token, error)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    Session session;
    session.user = user;
    session.fd = fd;
    sessions_[token] = session;
    fdToToken_[fd] = token;
    return true;
}

bool SessionStore::validate(const std::string &token, int fd, std::string &user) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto iterator = sessions_.find(token);
    if (iterator == sessions_.end() || iterator->second.fd != fd) {
        return false;
    }
    user = iterator->second.user;
    return true;
}

bool SessionStore::findUser(const std::string &token, std::string &user, int &fd) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto iterator = sessions_.find(token);
    if (iterator == sessions_.end()) {
        return false;
    }
    user = iterator->second.user;
    fd = iterator->second.fd;
    return true;
}

void SessionStore::removeFd(int fd)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto iterator = fdToToken_.find(fd);
    if (iterator == fdToToken_.end()) {
        return;
    }
    sessions_.erase(iterator->second);
    fdToToken_.erase(iterator);
}
