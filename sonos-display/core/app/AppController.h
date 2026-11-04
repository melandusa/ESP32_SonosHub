#pragma once

#include "app/AppState.h"
#include "app/IWorker.h"
#include "platform/IBacklight.h"
#include "platform/IClock.h"

#include <cstddef>
#include <cstdint>

namespace app {

// UI-thread side of the application: owns the state, turns user actions into worker
// commands, applies results, schedules polling and reconnects, and handles dimming.
// Free of LVGL so it can be unit tested.
class AppController {
public:
    AppController(IWorker& worker, const platform::IClock& clock, platform::IBacklight& backlight,
                  AppConfig config);

    void start();
    // Call regularly (every few 10 ms) from the UI thread.
    void tick();

    void playFavorite(std::size_t index);
    void stop();
    void volumeUp() { changeVolume(config_.volumeStep); }
    void volumeDown() { changeVolume(-config_.volumeStep); }
    void noteUserActivity();

    // Screen saver active (backlight off): the next touch must only wake the display.
    bool isScreenOff() const { return screenPower_ == ScreenPower::Off; }

    const AppState& state() const { return state_; }
    const AppConfig& config() const { return config_; }
    // Incremented on every state change; lets the UI skip redundant redraws.
    uint32_t version() const { return version_; }

private:
    // Returns the command's sequence number, 0 if the queue was full.
    uint32_t submit(Command command);
    void handleResult(const Result& result, uint64_t now);
    void handleDiscovery(const Result& result, uint64_t now);
    void handleStatus(const Result& result);
    void handleFavorites(const Result& result);
    void handleLogo(const Result& result, uint64_t now);
    void loadNextLogo(uint64_t now);
    void trackFailures(const Result& result, uint64_t now);
    void enterUnreachable(uint64_t now);

    void changeVolume(int delta);
    void flushVolume(uint64_t now);
    void updateActiveFavorite();
    void updateDimming(uint64_t now);
    void markChanged() { ++version_; }

    IWorker& worker_;
    const platform::IClock& clock_;
    platform::IBacklight& backlight_;
    AppConfig config_;
    AppState state_;
    uint32_t version_ = 0;
    uint32_t nextSeq_ = 1;

    std::string lastDeviceIp_;
    bool discoverInFlight_ = false;
    bool statusInFlight_ = false;
    bool favoritesInFlight_ = false;
    uint64_t nextDiscoveryMs_ = 0;
    uint64_t nextPollMs_ = 0;
    uint64_t nextFavoritesMs_ = 0;
    uint32_t backoffMs_ = 0;
    int consecutiveFailures_ = 0;
    int commandsInFlight_ = 0;
    uint64_t nextLogoMs_ = 0;
    uint64_t logoRequestedMs_ = 0;

    // Status results from commands issued before the latest local change are stale.
    uint32_t playbackChangeSeq_ = 0;
    uint32_t volumeChangeSeq_ = 0;

    int pendingVolume_ = -1;  // optimistic value not sent yet
    bool volumeSent_ = false;
    uint64_t lastVolumeSendMs_ = 0;

    enum class ScreenPower { On, Dimmed, Off };
    void setScreenPower(ScreenPower power);

    uint64_t lastActivityMs_ = 0;
    ScreenPower screenPower_ = ScreenPower::On;
};

} // namespace app
