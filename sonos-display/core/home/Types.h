#pragma once

#include <string>
#include <vector>

namespace home {

enum class DeviceKind { Switch, Shutter };

// How a shutter channel takes its level (depends on the channel type).
enum class ShutterControl { ShutterLevel, PrimaryShadingLevel };

// One controllable channel of a Homematic IP device; shown as one tile.
struct Device {
    std::string deviceId;
    int channel = 0;
    DeviceKind kind = DeviceKind::Switch;
    ShutterControl shutterControl = ShutterControl::ShutterLevel;
    std::string name;
    bool reachable = true;
    bool on = false;      // Switch
    double level = -1.0;  // Shutter: 0.0 open .. 1.0 closed, -1 unknown
    bool moving = false;  // Shutter

    bool sameChannel(const Device& other) const {
        return deviceId == other.deviceId && channel == other.channel;
    }
    bool operator==(const Device& other) const {
        return sameChannel(other) && kind == other.kind && shutterControl == other.shutterControl &&
               name == other.name && reachable == other.reachable && on == other.on &&
               level == other.level && moving == other.moving;
    }
    bool operator!=(const Device& other) const { return !(*this == other); }
};

using DeviceList = std::vector<Device>;

enum class HomeStatus {
    Ok,
    NetworkError,  // no connection or connection lost
    AuthError,     // the HCU rejected the authorization token
    Rejected,      // the HCU answered the request with an error
};

const char* homeStatusName(HomeStatus status);

} // namespace home
