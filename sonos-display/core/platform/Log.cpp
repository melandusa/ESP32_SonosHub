#include "platform/Log.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>

namespace platform {

namespace {
std::atomic<ILogger*> gLogger{nullptr};
std::atomic<int> gMinLevel{static_cast<int>(LogLevel::Info)};
} // namespace

void setLogger(ILogger* logger) { gLogger = logger; }

void setMinLogLevel(LogLevel level) { gMinLevel = static_cast<int>(level); }

bool isLogEnabled(LogLevel level) {
    return gLogger.load() != nullptr && static_cast<int>(level) >= gMinLevel.load();
}

void logf(LogLevel level, const char* fmt, ...) {
    if (!isLogEnabled(level)) return;
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if (ILogger* logger = gLogger.load()) logger->write(level, buffer);
}

} // namespace platform
