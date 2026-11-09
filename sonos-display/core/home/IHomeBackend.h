#pragma once

#include "home/Types.h"

namespace home {

// Blocking connection to the smart home system, used by the home worker only.
// Real implementation: hcu::HcuClient; the Linux simulation has a mock.
class IHomeBackend {
public:
    virtual ~IHomeBackend() = default;

    // (Re)connects and loads all devices.
    virtual HomeStatus connect() = 0;
    virtual bool isConnected() const = 0;
    // Waits up to timeoutMs for state changes and applies them.
    virtual HomeStatus poll(int timeoutMs) = 0;

    virtual HomeStatus setSwitch(const Device& device, bool on) = 0;
    virtual HomeStatus setShutterLevel(const Device& device, double level) = 0;
    virtual HomeStatus stopShutter(const Device& device) = 0;

    // Current state of all controllable channels, sorted by name.
    virtual const DeviceList& devices() const = 0;
};

} // namespace home
