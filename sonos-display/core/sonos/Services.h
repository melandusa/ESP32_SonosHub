#pragma once

namespace sonos {

struct ServiceInfo {
    const char* controlPath;
    const char* serviceType;
};

constexpr ServiceInfo kContentDirectory{"/MediaServer/ContentDirectory/Control",
                                        "urn:schemas-upnp-org:service:ContentDirectory:1"};
constexpr ServiceInfo kAVTransport{"/MediaRenderer/AVTransport/Control",
                                   "urn:schemas-upnp-org:service:AVTransport:1"};
constexpr ServiceInfo kRenderingControl{"/MediaRenderer/RenderingControl/Control",
                                        "urn:schemas-upnp-org:service:RenderingControl:1"};

constexpr int kSonosPort = 1400;

} // namespace sonos
