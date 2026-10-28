#pragma once

#include "platform/ILogger.h"

namespace platform {

// Installs the process-wide logger. Without a logger, messages are dropped.
void setLogger(ILogger* logger);
void setMinLogLevel(LogLevel level);
bool isLogEnabled(LogLevel level);

#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
void logf(LogLevel level, const char* fmt, ...);

} // namespace platform

#define LOGD(...) ::platform::logf(::platform::LogLevel::Debug, __VA_ARGS__)
#define LOGI(...) ::platform::logf(::platform::LogLevel::Info, __VA_ARGS__)
#define LOGW(...) ::platform::logf(::platform::LogLevel::Warn, __VA_ARGS__)
#define LOGE(...) ::platform::logf(::platform::LogLevel::Error, __VA_ARGS__)
