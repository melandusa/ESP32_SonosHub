#pragma once

#include "app/BasicWorker.h"
#include "app/Commands.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>

namespace linux_platform {

// Runs commands on a background std::thread, one at a time, in submission order.
template <typename CommandT, typename ResultT>
class BasicThreadWorker : public app::BasicWorker<CommandT, ResultT> {
public:
    using Executor = std::function<ResultT(const CommandT&)>;

    explicit BasicThreadWorker(Executor executor, std::size_t maxQueued = 16)
        : executor_(std::move(executor)), maxQueued_(maxQueued), thread_([this] { run(); }) {}

    ~BasicThreadWorker() override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
            commands_.clear();
        }
        wake_.notify_one();
        thread_.join();
    }

    BasicThreadWorker(const BasicThreadWorker&) = delete;
    BasicThreadWorker& operator=(const BasicThreadWorker&) = delete;

    bool submit(const CommandT& command) override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (commands_.size() >= maxQueued_) return false;
            commands_.push_back(command);
        }
        wake_.notify_one();
        return true;
    }

    bool poll(ResultT& result) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (results_.empty()) return false;
        result = std::move(results_.front());
        results_.pop_front();
        return true;
    }

private:
    void run() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (true) {
            wake_.wait(lock, [this] { return stopping_ || !commands_.empty(); });
            if (stopping_) return;
            const CommandT command = std::move(commands_.front());
            commands_.pop_front();

            lock.unlock();
            ResultT result = executor_(command);
            lock.lock();

            results_.push_back(std::move(result));
        }
    }

    Executor executor_;
    std::size_t maxQueued_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<CommandT> commands_;
    std::deque<ResultT> results_;
    bool stopping_ = false;
    std::thread thread_;  // last member: starts after everything else is initialized
};

using ThreadWorker = BasicThreadWorker<app::Command, app::Result>;

} // namespace linux_platform
