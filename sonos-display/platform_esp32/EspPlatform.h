#pragma once

#include "platform/IBacklight.h"
#include "platform/IClock.h"
#include "platform/ILogger.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace esp_platform {

// Writes to Serial; safe to call from the UI loop and the worker task.
class SerialLogger : public platform::ILogger {
public:
    SerialLogger();
    void write(platform::LogLevel level, const char* message) override;

private:
    SemaphoreHandle_t mutex_;
};

class EspClock : public platform::IClock {
public:
    uint64_t nowMs() const override;
};

// Backlight brightness via LEDC PWM
class PwmBacklight : public platform::IBacklight {
public:
    explicit PwmBacklight(int pin);
    void begin();
    void setLevel(int level) override;

private:
    int pin_;
};

} // namespace esp_platform
