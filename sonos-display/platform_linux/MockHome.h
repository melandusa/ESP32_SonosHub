#pragma once

#include "home/IHomeBackend.h"

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>

namespace linux_platform {

// Simulated Homematic IP system: three shutters that move over a few seconds and four
// sockets (more than one page). setOnline(false) simulates an unreachable HCU.
class MockHome : public home::IHomeBackend {
public:
    MockHome();

    home::HomeStatus connect() override;
    bool isConnected() const override { return connected_; }
    home::HomeStatus poll(int timeoutMs) override;
    home::HomeStatus setSwitch(const home::Device& device, bool on) override;
    home::HomeStatus setShutterLevel(const home::Device& device, double level) override;
    home::HomeStatus stopShutter(const home::Device& device) override;
    const home::DeviceList& devices() const override { return devices_; }

    void setOnline(bool online) { online_ = online; }
    bool isOnline() const { return online_; }

private:
    home::Device* find(const home::Device& device);
    bool checkOnline();
    void simulate();

    std::atomic<bool> online_{true};
    bool connected_ = false;
    home::DeviceList all_;      // the simulated system
    home::DeviceList devices_;  // what the client has seen (copy of all_ while connected)
    std::map<std::string, double> targets_;
    std::chrono::steady_clock::time_point lastStep_;
};

} // namespace linux_platform
