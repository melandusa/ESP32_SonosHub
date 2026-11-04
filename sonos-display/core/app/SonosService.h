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

    // Enables LoadLogo; without a decoder logos are reported as unavailable.
    void setLogoDecoder(LogoDecoder decoder, int sizePx);

    Result execute(const Command& command);

private:
    void discover(const Command& command, Result& result);
    void loadFavorites(Result& result);
    void refreshStatus(Result& result);
    void loadLogo(const Command& command, Result& result);

    sonos::SonosClient& client_;
    platform::IDiscovery& discovery_;
    std::string fixedIp_;
    LogoDecoder logoDecoder_;
    int logoSizePx_ = 0;
};

} // namespace app
