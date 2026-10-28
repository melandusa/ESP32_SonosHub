#pragma once

#include "app/Commands.h"
#include "platform/IDiscovery.h"
#include "sonos/SonosClient.h"

#include <string>

namespace app {

// Blocking command executor. Runs in the worker context only.
class SonosService {
public:
    SonosService(sonos::SonosClient& client, platform::IDiscovery& discovery, std::string fixedIp);

    Result execute(const Command& command);

private:
    void discover(const Command& command, Result& result);
    void loadFavorites(Result& result);
    void refreshStatus(Result& result);

    sonos::SonosClient& client_;
    platform::IDiscovery& discovery_;
    std::string fixedIp_;
};

} // namespace app
