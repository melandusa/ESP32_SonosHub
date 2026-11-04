#include "app/SonosService.h"

#include "platform/Log.h"

#include <algorithm>

namespace app {

namespace {
constexpr int kDiscoveryTimeoutMs = 1500;
constexpr std::size_t kMaxLogoBytes = 512 * 1024;
}

SonosService::SonosService(sonos::SonosClient& client, platform::IDiscovery& discovery,
                           std::string fixedIp)
    : client_(client), discovery_(discovery), fixedIp_(std::move(fixedIp)) {}

void SonosService::setLogoDecoder(LogoDecoder decoder, int sizePx) {
    logoDecoder_ = std::move(decoder);
    logoSizePx_ = sizePx;
}

Result SonosService::execute(const Command& command) {
    Result result;
    result.type = command.type;
    result.seq = command.seq;

    switch (command.type) {
    case CommandType::Discover:
        discover(command, result);
        break;
    case CommandType::LoadFavorites:
        loadFavorites(result);
        break;
    case CommandType::RefreshStatus:
        refreshStatus(result);
        break;
    case CommandType::PlayFavorite:
        result.status = client_.setAVTransportUri(command.uri, command.metadata);
        if (result.ok()) result.status = client_.play();
        break;
    case CommandType::SetVolume:
        result.status = client_.setVolume(command.volume);
        break;
    case CommandType::Stop:
        result.status = client_.stop();
        break;
    case CommandType::LoadLogo:
        loadLogo(command, result);
        break;
    }

    if (!result.ok()) {
        LOGD("%s failed: %s", commandName(command.type), sonos::callStatusName(result.status));
    }
    return result;
}

void SonosService::discover(const Command& command, Result& result) {
    std::vector<std::string> candidates;
    if (!fixedIp_.empty()) {
        candidates.push_back(fixedIp_);
    } else {
        candidates = discovery_.discover(kDiscoveryTimeoutMs);
        LOGI("SSDP: %zu ZonePlayer(s) found", candidates.size());
        // Prefer the device we were connected to before, otherwise keep SSDP order.
        auto it = std::find(candidates.begin(), candidates.end(), command.preferredIp);
        if (it != candidates.end()) std::rotate(candidates.begin(), it, it + 1);
    }

    result.status = sonos::CallStatus::NetworkError;
    for (const std::string& ip : candidates) {
        sonos::DeviceInfo info;
        const sonos::CallStatus status = client_.fetchDeviceInfo(ip, info);
        if (status != sonos::CallStatus::Ok) {
            LOGI("  %s: %s", ip.c_str(), sonos::callStatusName(status));
            continue;
        }
        // Surround speakers and subs of a home theater set (and stereo pair secondaries) are
        // players too, but reject transport commands. Probe before choosing one.
        bool playable = false;
        if (info.isPlayer) {
            client_.setDevice(ip);
            sonos::TransportState state;
            playable = client_.getTransportInfo(state) == sonos::CallStatus::Ok;
        }
        LOGI("  %s: \"%s\" (%s)%s", ip.c_str(), info.roomName.c_str(), info.modelName.c_str(),
             playable ? "" : " - not controllable, skipped");
        if (!playable || !result.deviceIp.empty()) continue;
        // Multi-room is out of scope: the first playable device wins, the rest is only logged.
        // TODO(groups): resolve the group coordinator via ZoneGroupTopology.
        result.status = sonos::CallStatus::Ok;
        result.deviceIp = ip;
        result.roomName = info.roomName;
    }

    client_.setDevice(result.ok() ? result.deviceIp : std::string());
}

void SonosService::loadFavorites(Result& result) {
    sonos::FavoritesParseResult parsed;
    result.status = client_.browseFavorites(parsed);
    if (!result.ok()) return;
    for (const std::string& title : parsed.skippedTitles) {
        LOGI("Favorite \"%s\" is not a radio station - hidden", title.c_str());
    }
    result.favorites = std::move(parsed.radio);
}

void SonosService::refreshStatus(Result& result) {
    result.status = client_.getTransportInfo(result.info.transport);
    if (result.ok()) result.status = client_.getVolume(result.info.volume);
    // Media info is read on every poll: a station change from the Sonos app often keeps
    // the transport state at PLAYING, so it would go unnoticed otherwise.
    if (result.ok()) result.status = client_.getMediaInfo(result.info.media);
}

void SonosService::loadLogo(const Command& command, Result& result) {
    result.uri = command.uri;
    if (!logoDecoder_) {
        result.status = sonos::CallStatus::ParseError;
        return;
    }
    std::string encoded;
    result.status = client_.fetchAlbumArt(command.uri, kMaxLogoBytes, encoded);
    if (!result.ok()) return;

    auto logo = std::make_shared<LogoImage>();
    if (!logoDecoder_(encoded, logoSizePx_, *logo)) {
        LOGD("Logo for %s: cannot decode %zu bytes", command.uri.c_str(), encoded.size());
        result.status = sonos::CallStatus::ParseError;
        return;
    }
    result.logo = std::move(logo);
}

} // namespace app
