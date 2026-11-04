#include "WifiUdpSsdpDiscovery.h"

#include "platform/Log.h"
#include "sonos/Ssdp.h"

#include <Arduino.h>
#include <WiFiUdp.h>

#include <algorithm>
#include <cstring>

namespace esp_platform {

std::vector<std::string> WifiUdpSsdpDiscovery::discover(int timeoutMs) {
    std::vector<std::string> found;
    WiFiUDP udp;
    if (!udp.begin(0)) {
        LOGE("SSDP: UDP socket failed");
        return found;
    }

    IPAddress target;
    target.fromString(sonos::kSsdpMulticastAddress);
    // UDP may get lost: send the request twice.
    for (int i = 0; i < 2; ++i) {
        udp.beginPacket(target, sonos::kSsdpPort);
        udp.write(reinterpret_cast<const uint8_t*>(sonos::kSsdpSearchRequest),
                  std::strlen(sonos::kSsdpSearchRequest));
        udp.endPacket();
    }

    char buffer[1024];
    const uint32_t start = millis();
    while (millis() - start < static_cast<uint32_t>(timeoutMs)) {
        const int size = udp.parsePacket();
        if (size <= 0) {
            delay(10);
            continue;
        }
        const int length = udp.read(buffer, sizeof(buffer) - 1);
        if (length <= 0) continue;
        const std::string ip = sonos::parseSsdpResponse(std::string(buffer, static_cast<size_t>(length)));
        if (!ip.empty() && std::find(found.begin(), found.end(), ip) == found.end()) {
            LOGD("SSDP: response from %s", ip.c_str());
            found.push_back(ip);
        }
    }

    udp.stop();
    return found;
}

} // namespace esp_platform
