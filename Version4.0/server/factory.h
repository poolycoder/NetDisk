#pragma once

#include "workQue.h"

#include <functional>
#include <thread>
#include <vector>

class Factory {
public:
    using TaskHandler = std::function<void(const Task &)>;

    Factory(int threadCount, TaskHandler handler);
    ~Factory();

    Factory(const Factory &) = delete;
    Factory &operator=(const Factory &) = delete;

    void start();
    void enqueue(const Task &task);
    void stop();

private:
    void workerLoop();

    int threadCount_;
    TaskHandler handler_;
    WorkQue queue_;
    std::vector<std::thread> workers_;
    bool started_ = false;
};
