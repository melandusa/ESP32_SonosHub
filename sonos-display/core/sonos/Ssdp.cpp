#include "sonos/Ssdp.h"

#include <algorithm>
#include <cctype>

namespace sonos {

const char* const kSsdpSearchRequest =
    "M-SEARCH * HTTP/1.1\r\n"
    "HOST: 239.255.255.250:1900\r\n"
    "MAN: \"ssdp:discover\"\r\n"
    "MX: 1\r\n"
    "ST: urn:schemas-upnp-org:device:ZonePlayer:1\r\n"
    "\r\n";

namespace {

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// Value of a header line (case-insensitive name), trimmed; "" if missing.
std::string headerValue(const std::string& response, const std::string& name) {
    const std::string lower = toLower(response);
    const std::string key = "\n" + toLower(name) + ":";
    const auto pos = lower.find(key);
    if (pos == std::string::npos) return {};
    auto start = pos + key.size();
    auto end = response.find_first_of("\r\n", start);
    if (end == std::string::npos) end = response.size();
    while (start < end && response[start] == ' ') ++start;
    return response.substr(start, end - start);
}

} // namespace

std::string parseSsdpResponse(const std::string& response) {
    if (headerValue(response, "ST").find("ZonePlayer") == std::string::npos) return {};
    const std::string location = headerValue(response, "LOCATION");  // http://<ip>:1400/xml/...
    const std::string prefix = "http://";
    if (location.compare(0, prefix.size(), prefix) != 0) return {};
    const auto hostEnd = location.find_first_of(":/", prefix.size());
    return location.substr(prefix.size(), hostEnd - prefix.size());
}

} // namespace sonos
