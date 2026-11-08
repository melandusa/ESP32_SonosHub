#include "sonos/Types.h"

#include <cctype>

namespace sonos {

TransportState parseTransportState(const std::string& value) {
    if (value == "PLAYING") return TransportState::Playing;
    if (value == "STOPPED") return TransportState::Stopped;
    if (value == "PAUSED_PLAYBACK") return TransportState::Paused;
    if (value == "TRANSITIONING") return TransportState::Transitioning;
    return TransportState::Unknown;
}

MediaSource mediaSourceOf(const std::string& uri) {
    if (uri.rfind("x-sonos-htastream:", 0) == 0) return MediaSource::TvInput;  // HDMI ARC / optical
    if (uri.rfind("x-rincon-stream:", 0) == 0) return MediaSource::LineIn;     // analog input
    return MediaSource::Other;
}

bool isSameStream(const std::string& a, const std::string& b) {
    if (a.empty() || b.empty()) return false;
    const std::size_t lengthA = a.find('?') == std::string::npos ? a.size() : a.find('?');
    const std::size_t lengthB = b.find('?') == std::string::npos ? b.size() : b.find('?');
    if (lengthA != lengthB) return false;
    for (std::size_t i = 0; i < lengthA; ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

const char* transportStateName(TransportState state) {
    switch (state) {
    case TransportState::Stopped: return "STOPPED";
    case TransportState::Playing: return "PLAYING";
    case TransportState::Paused: return "PAUSED_PLAYBACK";
    case TransportState::Transitioning: return "TRANSITIONING";
    case TransportState::Unknown: break;
    }
    return "UNKNOWN";
}

} // namespace sonos
