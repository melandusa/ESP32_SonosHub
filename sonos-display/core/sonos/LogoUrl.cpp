#include "sonos/LogoUrl.h"

#include <cstdio>

namespace sonos {

std::string percentEncode(const std::string& text) {
    std::string out;
    out.reserve(text.size() * 3);
    for (unsigned char c : text) {
        const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        if (unreserved) {
            out += static_cast<char>(c);
        } else {
            char encoded[4];
            std::snprintf(encoded, sizeof(encoded), "%%%02X", c);
            out += encoded;
        }
    }
    return out;
}

std::string albumArtProxyUrl(const std::string& speakerBaseUrl, const std::string& streamUri) {
    return speakerBaseUrl + "/getaa?s=1&u=" + percentEncode(streamUri);
}

} // namespace sonos
