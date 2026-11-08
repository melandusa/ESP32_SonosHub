#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace esp_platform {

// Firmware updates from GitHub releases. Checks and downloads run in a task of their own,
// so neither the UI nor the Sonos worker waits for them. The UI thread decides when to
// check and when to install (only while nobody uses the display).
class OtaUpdater {
public:
    enum class State { Idle, Checking, Available, Installing, Installed, Failed };

    OtaUpdater(std::string manifestUrl, int currentVersion);
    void begin();

    // Both ignored while a check or download is running.
    void checkNow();
    void install();  // only in state Available

    State state() const { return state_.load(); }
    int availableVersion() const { return availableVersion_.load(); }
    int progressPercent() const { return progress_.load(); }

    // After an update, the new firmware runs on probation: confirm once it reached the
    // WLAN (so it can update itself again), otherwise the bootloader returns to the old one.
    static bool isPendingVerification();
    static void confirmFirmware();
    static void rejectFirmware();  // reboots into the previous firmware

private:
    enum class Request : uint8_t { None, Check, Install };

    static void taskMain(void* arg);
    void runCheck();
    void runInstall();

    const std::string manifestUrl_;
    const int currentVersion_;
    std::string firmwareUrl_;  // set by the task before it publishes State::Available
    std::atomic<State> state_{State::Idle};
    std::atomic<Request> request_{Request::None};
    std::atomic<int> availableVersion_{0};
    std::atomic<int> progress_{0};
    TaskHandle_t task_ = nullptr;
};

} // namespace esp_platform
