#pragma once

#include "home/Commands.h"
#include "platform/IClock.h"

#include <cstddef>
#include <cstdint>

namespace home {

enum class HomeConnection { Connecting, Connected, Unreachable, AuthFailed };

struct HomeConfig {
    uint32_t reconnectBackoffMinMs = 5000;
    uint32_t reconnectBackoffMaxMs = 60000;
    uint32_t authRetryMs = 5 * 60 * 1000;  // token rejected: retry rarely
};

struct HomeState {
    HomeConnection connection = HomeConnection::Connecting;
    DeviceList devices;
};

// UI-thread side of the Homematic IP part: keeps a poll running on the home worker
// (which receives the HCU's push events), turns taps into commands and reconnects.
// Free of LVGL so it can be unit tested.
class HomeController {
public:
    HomeController(HomeWorker& worker, const platform::IClock& clock, HomeConfig config = {});

    void start();
    // Call regularly from the UI thread.
    void tick();

    void toggleSwitch(std::size_t index);
    void openShutter(std::size_t index) { moveShutter(index, 0.0); }
    void closeShutter(std::size_t index) { moveShutter(index, 1.0); }
    void stopShutter(std::size_t index);

    const HomeState& state() const { return state_; }
    uint32_t version() const { return version_; }

private:
    bool submit(HomeCommand command);
    const Device* controllable(std::size_t index, DeviceKind kind) const;
    void moveShutter(std::size_t index, double level);
    void handleResult(const HomeResult& result, uint64_t now);
    void setConnection(HomeConnection connection);
    void markChanged() { ++version_; }

    HomeWorker& worker_;
    const platform::IClock& clock_;
    HomeConfig config_;
    HomeState state_;
    uint32_t version_ = 0;
    uint32_t nextSeq_ = 1;
    bool started_ = false;
    int inFlight_ = 0;
    bool connectInFlight_ = false;
    bool pollInFlight_ = false;
    uint64_t nextConnectMs_ = 0;
    uint32_t backoffMs_ = 0;
    // Device lists from commands issued before the latest tap would undo its optimistic update.
    uint32_t controlSeq_ = 0;
};

} // namespace home
