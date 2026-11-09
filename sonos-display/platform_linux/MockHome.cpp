#include "MockHome.h"

#include "hcu/HcuClient.h"
#include "platform/Log.h"

#include <algorithm>
#include <cmath>
#include <thread>

namespace linux_platform {

using home::Device;
using home::DeviceKind;
using home::HomeStatus;

namespace {

constexpr double kShutterSpeedPerSecond = 0.2;  // 5 s from open to closed

Device makeDevice(const char* id, const char* name, DeviceKind kind, bool on, double level) {
    Device d;
    d.deviceId = id;
    d.channel = 1;
    d.kind = kind;
    d.name = name;
    d.on = on;
    d.level = kind == DeviceKind::Shutter ? level : -1.0;
    return d;
}

} // namespace

MockHome::MockHome() {
    all_ = {
        makeDevice("3014F711A000000000000001", "Rollladen Wohnzimmer", DeviceKind::Shutter, false, 0.0),
        makeDevice("3014F711A000000000000002", "Rollladen Küche", DeviceKind::Shutter, false, 1.0),
        makeDevice("3014F711A000000000000003", "Rollladen Schlafzimmer", DeviceKind::Shutter, false, 0.4),
        makeDevice("3014F711A000000000000004", "Stehlampe", DeviceKind::Switch, true, 0),
        makeDevice("3014F711A000000000000005", "Kaffeemaschine", DeviceKind::Switch, false, 0),
        makeDevice("3014F711A000000000000006", "Lichterkette Balkon", DeviceKind::Switch, false, 0),
        makeDevice("3014F711A000000000000007", "Ventilator", DeviceKind::Switch, false, 0),
    };
    hcu::sortDevices(all_);
    lastStep_ = std::chrono::steady_clock::now();
}

bool MockHome::checkOnline() {
    if (online_) return true;
    connected_ = false;
    return false;
}

Device* MockHome::find(const Device& device) {
    for (Device& d : all_) {
        if (d.sameChannel(device)) return &d;
    }
    return nullptr;
}

void MockHome::simulate() {
    const auto now = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(now - lastStep_).count();
    lastStep_ = now;
    for (Device& d : all_) {
        auto target = targets_.find(d.deviceId);
        if (target == targets_.end()) continue;
        const double step = kShutterSpeedPerSecond * seconds;
        if (std::abs(target->second - d.level) <= step) {
            d.level = target->second;
            d.moving = false;
            targets_.erase(target);
        } else {
            d.level += target->second > d.level ? step : -step;
            d.moving = true;
        }
    }
    devices_ = all_;
}

HomeStatus MockHome::connect() {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (!checkOnline()) return HomeStatus::NetworkError;
    connected_ = true;
    simulate();
    return HomeStatus::Ok;
}

HomeStatus MockHome::poll(int timeoutMs) {
    std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
    if (!checkOnline()) return HomeStatus::NetworkError;
    simulate();
    return HomeStatus::Ok;
}

HomeStatus MockHome::setSwitch(const Device& device, bool on) {
    if (!checkOnline()) return HomeStatus::NetworkError;
    Device* d = find(device);
    if (!d) return HomeStatus::Rejected;
    LOGI("Mock HCU: %s %s", d->name.c_str(), on ? "on" : "off");
    d->on = on;
    simulate();
    return HomeStatus::Ok;
}

HomeStatus MockHome::setShutterLevel(const Device& device, double level) {
    if (!checkOnline()) return HomeStatus::NetworkError;
    Device* d = find(device);
    if (!d) return HomeStatus::Rejected;
    LOGI("Mock HCU: %s to %.0f %%", d->name.c_str(), level * 100);
    simulate();
    targets_[d->deviceId] = level;
    d->moving = d->level != level;
    devices_ = all_;
    return HomeStatus::Ok;
}

HomeStatus MockHome::stopShutter(const Device& device) {
    if (!checkOnline()) return HomeStatus::NetworkError;
    Device* d = find(device);
    if (!d) return HomeStatus::Rejected;
    LOGI("Mock HCU: stop %s", d->name.c_str());
    simulate();
    targets_.erase(d->deviceId);
    d->moving = false;
    devices_ = all_;
    return HomeStatus::Ok;
}

} // namespace linux_platform
