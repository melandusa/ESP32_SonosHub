#pragma once

#include <string>

namespace sonos {

struct Favorite {
    std::string title;
    std::string uri;          // value of <res>, sent as CurrentURI
    std::string metadata;     // decoded <r:resMD>, sent as CurrentURIMetaData
    std::string albumArtUri;  // absolute URL or empty
};

enum class TransportState { Unknown, Stopped, Playing, Paused, Transitioning };

struct MediaInfo {
    std::string uri;
    std::string title;
};

TransportState parseTransportState(const std::string& value);

// True if both URIs point to the same stream. Sonos reports the playing URI with other
// query flags and percent-encoding case than the favorite, so only the part before '?'
// is compared, case-insensitively.
bool isSameStream(const std::string& a, const std::string& b);
const char* transportStateName(TransportState state);

} // namespace sonos
