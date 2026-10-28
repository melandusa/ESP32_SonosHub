#include "LinuxPlatform.h"

#include "platform/Log.h"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace linux_platform {

void StdoutLogger::write(platform::LogLevel level, const char* message) {
    static const char* const kLevels[] = {"DEBUG", "INFO ", "WARN ", "ERROR"};
    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    const auto millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    std::tm local{};
    localtime_r(&seconds, &local);

    std::lock_guard<std::mutex> lock(mutex_);
    std::printf("%02d:%02d:%02d.%03d %s %s\n", local.tm_hour, local.tm_min, local.tm_sec,
                static_cast<int>(millis), kLevels[static_cast<int>(level)], message);
    std::fflush(stdout);
}

uint64_t SteadyClock::nowMs() const {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count());
}

void LogBacklight::setLevel(int level) { LOGI("Backlight -> %d %%", level); }

} // namespace linux_platform
