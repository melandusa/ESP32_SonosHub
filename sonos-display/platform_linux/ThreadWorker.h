#pragma once

#include "app/IWorker.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace linux_platform {

// Runs commands on a background std::thread, one at a time, in submission order.
class ThreadWorker : public app::IWorker {
public:
    using Executor = std::function<app::Result(const app::Command&)>;

    explicit ThreadWorker(Executor executor, std::size_t maxQueued = 16);
    ~ThreadWorker() override;

    ThreadWorker(const ThreadWorker&) = delete;
    ThreadWorker& operator=(const ThreadWorker&) = delete;

    bool submit(const app::Command& command) override;
    bool poll(app::Result& result) override;

private:
    void run();

    Executor executor_;
    std::size_t maxQueued_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<app::Command> commands_;
    std::deque<app::Result> results_;
    bool stopping_ = false;
    std::thread thread_;
};

} // namespace linux_platform
