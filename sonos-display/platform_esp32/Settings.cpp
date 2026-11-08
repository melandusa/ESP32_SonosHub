#include "Settings.h"

#include <Preferences.h>

namespace esp_platform {

namespace {
constexpr const char* kNamespace = "sonos";
}

void Settings::load() {
    Preferences prefs;
    if (!prefs.begin(kNamespace, true)) return;  // nothing saved yet: defaults
    speakerIp = prefs.getString("speakerIp", speakerIp.c_str()).c_str();
    maxVolume = prefs.getInt("maxVolume", maxVolume);
    prefs.end();
}

void Settings::save() const {
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) return;
    prefs.putString("speakerIp", speakerIp.c_str());
    prefs.putInt("maxVolume", maxVolume);
    prefs.end();
}

} // namespace esp_platform
