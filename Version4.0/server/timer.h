#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

class Timer {
public:
    using TimeoutHandler = std::function<void(int)>;

    Timer(int timeoutSeconds, TimeoutHandler handler);
    ~Timer();

    Timer(const Timer &) = delete;
    Timer &operator=(const Timer &) = delete;

    void add(int fd);
    void update(int fd);
    void remove(int fd);
    void stop();

private:
    void run();

    int timeoutSeconds_;
    TimeoutHandler handler_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::map<int, std::chrono::steady_clock::time_point> clients_;
    std::thread thread_;
    bool stopped_ = false;
};
