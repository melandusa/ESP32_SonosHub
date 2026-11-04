#pragma once

#include "app/LogoImage.h"
#include "sonos/Types.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace app {

enum class Connection { Connecting, Connected, Unreachable };

struct AppConfig {
    int maxVolume = 60;
    int volumeStep = 1;
    uint32_t pollIntervalMs = 2000;
    uint32_t favoritesIntervalMs = 10 * 60 * 1000;
    uint32_t volumeDebounceMs = 200;
    int failuresUntilUnreachable = 3;
    uint32_t reconnectBackoffMinMs = 5000;
    uint32_t reconnectBackoffMaxMs = 30000;
    uint32_t dimAfterMs = 2 * 60 * 1000;
    uint32_t screenOffAfterMs = 10 * 60 * 1000;  // screen saver: backlight off
    int backlightBright = 100;
    int backlightDim = 15;
    int logoSizePx = 88;                  // station logos on the tiles, 0 disables them
    uint32_t logoRetryMs = 60 * 1000;     // after a network error
};

// Everything the UI shows. Owned by AppController, read in the UI thread.
struct AppState {
    Connection connection = Connection::Connecting;
    std::string roomName;
    sonos::TransportState transport = sonos::TransportState::Unknown;
    std::string currentTitle;
    std::string currentUri;
    int volume = 0;
    bool volumeKnown = false;
    std::vector<sonos::Favorite> favorites;
    int activeFavorite = -1;  // index into favorites matching the current station, -1 if none
    // Station logos by favorite URI. nullptr: the speaker has none, the tile shows text only.
    std::map<std::string, LogoPtr> logos;
};

} // namespace app
