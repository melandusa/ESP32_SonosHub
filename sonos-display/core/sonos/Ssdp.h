#pragma once

#include <string>

namespace sonos {

constexpr const char* kSsdpMulticastAddress = "239.255.255.250";
constexpr int kSsdpPort = 1900;

// M-SEARCH request for Sonos ZonePlayers
extern const char* const kSsdpSearchRequest;

// Extracts the host from the LOCATION header of an SSDP response, "" if not a ZonePlayer.
std::string parseSsdpResponse(const std::string& response);

} // namespace sonos
