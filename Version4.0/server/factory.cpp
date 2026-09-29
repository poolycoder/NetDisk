#include "factory.h"

#include <algorithm>
#include <utility>

Factory::Factory(int threadCount, TaskHandler handler)
    : threadCount_(std::max(1, threadCount)), handler_(std::move(handler))
{
}

Factory::~Factory()
{
    stop();
}

void Factory::start()
{
    if (started_) {
        return;
    }
    started_ = true;
    for (int i = 0; i < threadCount_; ++i) {
        workers_.emplace_back(&Factory::workerLoop, this);
    }
}

void Factory::enqueue(const Task &task)
{
    queue_.push(task);
}

void Factory::stop()
{
    if (!started_) {
        return;
    }
    queue_.stop();
    for (std::thread &worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
    started_ = false;
}

void Factory::workerLoop()
{
    Task task;
    while (queue_.pop(task)) {
        handler_(task);
    }
}
