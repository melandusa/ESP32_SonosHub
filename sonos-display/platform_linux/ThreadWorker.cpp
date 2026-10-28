#include "ThreadWorker.h"

namespace linux_platform {

ThreadWorker::ThreadWorker(Executor executor, std::size_t maxQueued)
    : executor_(std::move(executor)), maxQueued_(maxQueued), thread_([this] { run(); }) {}

ThreadWorker::~ThreadWorker() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        commands_.clear();
    }
    wake_.notify_one();
    thread_.join();
}

bool ThreadWorker::submit(const app::Command& command) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (commands_.size() >= maxQueued_) return false;
        commands_.push_back(command);
    }
    wake_.notify_one();
    return true;
}

bool ThreadWorker::poll(app::Result& result) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (results_.empty()) return false;
    result = std::move(results_.front());
    results_.pop_front();
    return true;
}

void ThreadWorker::run() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (true) {
        wake_.wait(lock, [this] { return stopping_ || !commands_.empty(); });
        if (stopping_) return;
        const app::Command command = std::move(commands_.front());
        commands_.pop_front();

        lock.unlock();
        app::Result result = executor_(command);
        lock.lock();

        results_.push_back(std::move(result));
    }
}

} // namespace linux_platform
