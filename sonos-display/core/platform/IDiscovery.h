#pragma once

#include <string>
#include <vector>

namespace platform {

class IDiscovery {
public:
    virtual ~IDiscovery() = default;
    // Returns the IPs of all ZonePlayers that answered within timeoutMs.
    virtual std::vector<std::string> discover(int timeoutMs) = 0;
};

} // namespace platform
