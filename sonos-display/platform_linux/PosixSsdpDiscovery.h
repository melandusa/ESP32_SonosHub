#pragma once

#include "platform/IDiscovery.h"

namespace linux_platform {

// SSDP M-SEARCH for Sonos ZonePlayers via UDP multicast.
class PosixSsdpDiscovery : public platform::IDiscovery {
public:
    std::vector<std::string> discover(int timeoutMs) override;
};

} // namespace linux_platform
