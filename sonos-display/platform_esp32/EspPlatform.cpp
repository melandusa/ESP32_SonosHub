#include "EspPlatform.h"

#include <Arduino.h>
#include <esp_timer.h>

namespace esp_platform {

namespace {
constexpr uint32_t kPwmFrequencyHz = 5000;
constexpr uint8_t kPwmResolutionBits = 10;
constexpr uint32_t kPwmMax = (1u << kPwmResolutionBits) - 1;
} // namespace

SerialLogger::SerialLogger() : mutex_(xSemaphoreCreateMutex()) {}

void SerialLogger::write(platform::LogLevel level, const char* message) {
    static const char* const kLevels[] = {"D", "I", "W", "E"};
    xSemaphoreTake(mutex_, portMAX_DELAY);
    Serial.printf("%8lu %s %s\n", static_cast<unsigned long>(millis()),
                  kLevels[static_cast<int>(level)], message);
    xSemaphoreGive(mutex_);
}

uint64_t EspClock::nowMs() const { return static_cast<uint64_t>(esp_timer_get_time() / 1000); }

PwmBacklight::PwmBacklight(int pin) : pin_(pin) {}

void PwmBacklight::begin() { ledcAttach(static_cast<uint8_t>(pin_), kPwmFrequencyHz, kPwmResolutionBits); }

void PwmBacklight::setLevel(int level) {
    if (level < 0) level = 0;
    if (level > 100) level = 100;
    ledcWrite(static_cast<uint8_t>(pin_), kPwmMax * static_cast<uint32_t>(level) / 100);
}

} // namespace esp_platform
