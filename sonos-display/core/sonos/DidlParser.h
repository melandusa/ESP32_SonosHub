#pragma once

#include "sonos/Types.h"

#include <string>
#include <vector>

namespace sonos {

// True for favorites that are live radio streams (supported in the MVP).
bool isRadioUri(const std::string& uri);

struct FavoritesParseResult {
    bool ok = false;
    std::vector<Favorite> radio;           // playable radio favorites, in Sonos order
    std::vector<std::string> skippedTitles;  // containers/playlists etc., hidden in the MVP
};

// Parses the (already once decoded) DIDL-Lite text from a Browse <Result>.
// baseUrl ("http://<ip>:1400") is prepended to relative album art paths.
FavoritesParseResult parseFavoritesDidl(const std::string& didl, const std::string& baseUrl);

// Extracts dc:title of the first item in a DIDL-Lite document, "" if none.
std::string parseDidlTitle(const std::string& didl);

} // namespace sonos
