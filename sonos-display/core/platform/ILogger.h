#pragma once

namespace platform {

enum class LogLevel { Debug, Info, Warn, Error };

// Implementations must be callable from any thread.
class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void write(LogLevel level, const char* message) = 0;
};

} // namespace platform
