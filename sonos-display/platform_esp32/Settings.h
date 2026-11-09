#pragma once

#include <string>

namespace esp_platform {

// User settings from the WLAN setup portal, stored in NVS (survive firmware updates).
struct Settings {
    std::string speakerIp;  // empty: find the speaker via SSDP
    int maxVolume = 60;
    std::string hcuHost;     // Homematic IP HCU; empty: no home page
    std::string hcuToken;    // Connect API authorization token from pairing
    std::string hcuPairKey;  // activation key entered in the portal, used once at the next start

    void load();
    void save() const;
};

} // namespace esp_platform
