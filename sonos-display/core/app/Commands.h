#pragma once

#include "app/LogoImage.h"
#include "sonos/SonosClient.h"
#include "sonos/Types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace app {

enum class CommandType { Discover, LoadFavorites, RefreshStatus, PlayFavorite, SetVolume, Stop, LoadLogo };

const char* commandName(CommandType type);

// Sent from the UI thread to the worker.
struct Command {
    CommandType type = CommandType::RefreshStatus;
    uint32_t seq = 0;         // assigned by AppController, increases monotonically
    std::string uri;          // PlayFavorite, LoadLogo: stream URI
    std::string metadata;     // PlayFavorite
    int volume = 0;           // SetVolume
    std::string preferredIp;  // Discover: last known device, chosen if found again
};

struct StatusInfo {
    sonos::TransportState transport = sonos::TransportState::Unknown;
    int volume = 0;
    sonos::MediaInfo media;
};

// Sent from the worker back to the UI thread.
struct Result {
    CommandType type = CommandType::RefreshStatus;
    uint32_t seq = 0;  // copied from the command
    sonos::CallStatus status = sonos::CallStatus::Ok;

    StatusInfo info;                       // RefreshStatus
    std::vector<sonos::Favorite> favorites;  // LoadFavorites
    std::string deviceIp;                  // Discover
    std::string roomName;                  // Discover
    std::string uri;                       // LoadLogo: copied from the command
    LogoPtr logo;                          // LoadLogo

    bool ok() const { return status == sonos::CallStatus::Ok; }
    bool networkFailed() const { return status == sonos::CallStatus::NetworkError; }
};

} // namespace app
