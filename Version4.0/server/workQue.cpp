#include "workQue.h"

void WorkQue::push(const Task &task)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) {
            return;
        }
        queue_.push_back(task);
    }
    condition_.notify_one();
}

bool WorkQue::pop(Task &task)
{
    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this] { return stopped_ || !queue_.empty(); });
    if (queue_.empty()) {
        return false;
    }
    task = queue_.front();
    queue_.pop_front();
    return true;
}

void WorkQue::stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    condition_.notify_all();
}
