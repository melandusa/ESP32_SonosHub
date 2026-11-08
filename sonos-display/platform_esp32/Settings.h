#pragma once

#include <string>

namespace esp_platform {

// User settings from the WLAN setup portal, stored in NVS (survive firmware updates).
struct Settings {
    std::string speakerIp;  // empty: find the speaker via SSDP
    int maxVolume = 60;

    void load();
    void save() const;
};

} // namespace esp_platform
