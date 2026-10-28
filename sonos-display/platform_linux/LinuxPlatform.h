#pragma once

#include "platform/IBacklight.h"
#include "platform/IClock.h"
#include "platform/ILogger.h"

#include <mutex>

namespace linux_platform {

class StdoutLogger : public platform::ILogger {
public:
    void write(platform::LogLevel level, const char* message) override;

private:
    std::mutex mutex_;
};

class SteadyClock : public platform::IClock {
public:
    uint64_t nowMs() const override;
};

// The simulator has no backlight; only log what the display would do.
class LogBacklight : public platform::IBacklight {
public:
    void setLevel(int level) override;
};

} // namespace linux_platform
