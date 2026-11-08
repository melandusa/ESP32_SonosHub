#pragma once

#include <string>

namespace app {

// Release description published next to the firmware: {"version": 3, "url": "https://.../firmware.bin"}
struct UpdateManifest {
    int version = 0;
    std::string url;
};

// Minimal reader for the manifest written by the release workflow (no general JSON parser).
// Requires a positive version and an https URL.
bool parseUpdateManifest(const std::string& json, UpdateManifest& out);

} // namespace app
