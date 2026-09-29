#pragma once

#include "../common/connection.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

struct Task {
    netdisk::ConnectionPtr connection;
    std::string command;
    std::string user;
    std::string token;
    std::uint64_t directoryId = 0;
    std::string name;
    std::uint64_t offset = 0;
};

class WorkQue {
public:
    void push(const Task &task);
    bool pop(Task &task);
    void stop();

private:
    std::deque<Task> queue_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool stopped_ = false;
};
