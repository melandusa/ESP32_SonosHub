#pragma once

#include "platform/IDiscovery.h"

namespace linux_platform {

// SSDP M-SEARCH for Sonos ZonePlayers via UDP multicast.
class PosixSsdpDiscovery : public platform::IDiscovery {
public:
    std::vector<std::string> discover(int timeoutMs) override;
};

// Extracts the host from the LOCATION header of an SSDP response, "" if not a ZonePlayer.
std::string parseSsdpResponse(const std::string& response);

} // namespace linux_platform
