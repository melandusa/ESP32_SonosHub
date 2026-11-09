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
    // Added in a later version: missing in older NVS data (getString would log an error)
    if (prefs.isKey("hcuHost")) hcuHost = prefs.getString("hcuHost").c_str();
    if (prefs.isKey("hcuToken")) hcuToken = prefs.getString("hcuToken").c_str();
    if (prefs.isKey("hcuPairKey")) hcuPairKey = prefs.getString("hcuPairKey").c_str();
    prefs.end();
}

void Settings::save() const {
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) return;
    prefs.putString("speakerIp", speakerIp.c_str());
    prefs.putInt("maxVolume", maxVolume);
    prefs.putString("hcuHost", hcuHost.c_str());
    prefs.putString("hcuToken", hcuToken.c_str());
    prefs.putString("hcuPairKey", hcuPairKey.c_str());
    prefs.end();
}

} // namespace esp_platform
