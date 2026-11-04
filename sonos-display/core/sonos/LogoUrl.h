#pragma once

#include <string>

namespace sonos {

// The speaker's album art proxy for a stream: http://<ip>:1400/getaa?s=1&u=<stream uri>.
// Serves the station logo over plain HTTP from the local network (TuneIn: 600x600 PNG),
// 404 if the service has none. The favorites' own albumArtURIs mostly redirect to the same
// image via HTTPS, so the proxy is the simpler source.
std::string albumArtProxyUrl(const std::string& speakerBaseUrl, const std::string& streamUri);

std::string percentEncode(const std::string& text);

} // namespace sonos
