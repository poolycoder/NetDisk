#include "timer.h"

#include <utility>
#include <vector>

Timer::Timer(int timeoutSeconds, TimeoutHandler handler)
    : timeoutSeconds_(timeoutSeconds), handler_(std::move(handler)),
      thread_(&Timer::run, this)
{
}

Timer::~Timer()
{
    stop();
}

void Timer::add(int fd)
{
    update(fd);
}

void Timer::update(int fd)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_[fd] = std::chrono::steady_clock::now();
    }
    condition_.notify_one();
}

void Timer::remove(int fd)
{
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.erase(fd);
}

void Timer::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) {
            return;
        }
        stopped_ = true;
    }
    condition_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void Timer::run()
{
    while (true) {
        std::vector<int> expired;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (condition_.wait_for(lock, std::chrono::seconds(1),
                                    [this] { return stopped_; })) {
                return;
            }
            const auto now = std::chrono::steady_clock::now();
            for (const auto &client : clients_) {
                const auto age = std::chrono::duration_cast<std::chrono::seconds>(
                    now - client.second).count();
                if (age >= timeoutSeconds_) {
                    expired.push_back(client.first);
                }
            }
            for (int fd : expired) {
                clients_.erase(fd);
            }
        }

        for (int fd : expired) {
            handler_(fd);
        }
    }
}
