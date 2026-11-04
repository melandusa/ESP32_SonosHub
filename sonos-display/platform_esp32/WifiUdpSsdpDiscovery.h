#pragma once

#include "platform/IDiscovery.h"

namespace esp_platform {

// SSDP M-SEARCH for Sonos ZonePlayers via WiFiUDP.
class WifiUdpSsdpDiscovery : public platform::IDiscovery {
public:
    std::vector<std::string> discover(int timeoutMs) override;
};

} // namespace esp_platform
