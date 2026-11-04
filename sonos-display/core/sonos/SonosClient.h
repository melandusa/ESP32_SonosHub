#pragma once

#include "platform/IHttpClient.h"
#include "sonos/DeviceDescription.h"
#include "sonos/DidlParser.h"
#include "sonos/Services.h"
#include "sonos/SoapBuilder.h"
#include "sonos/SoapParser.h"
#include "sonos/Types.h"

#include <cstddef>
#include <string>

namespace sonos {

enum class CallStatus { Ok, NetworkError, SoapFault, ParseError };

const char* callStatusName(CallStatus status);

// Blocking UPnP/SOAP client for a single ZonePlayer. Not thread-safe; use from one thread.
// TODO(groups): commands must go to the group coordinator once grouping is supported.
class SonosClient {
public:
    explicit SonosClient(platform::IHttpClient& http, int timeoutMs = 2000);

    void setDevice(const std::string& ip) { ip_ = ip; }
    const std::string& deviceIp() const { return ip_; }
    std::string baseUrl() const;
    void setDumpXml(bool dump) { dumpXml_ = dump; }

    CallStatus fetchDeviceInfo(const std::string& ip, DeviceInfo& out);
    CallStatus browseFavorites(FavoritesParseResult& out);
    // Station logo from the speaker's art proxy (encoded image, usually PNG).
    // ParseError if the speaker has none (404) or it is larger than maxBytes.
    CallStatus fetchAlbumArt(const std::string& streamUri, std::size_t maxBytes, std::string& out);

    CallStatus setAVTransportUri(const std::string& uri, const std::string& metadata);
    CallStatus play();
    CallStatus stop();
    CallStatus getTransportInfo(TransportState& out);
    CallStatus getMediaInfo(MediaInfo& out);

    CallStatus getVolume(int& out);
    CallStatus setVolume(int volume);

private:
    CallStatus call(const ServiceInfo& service, const char* action, const SoapArgs& args,
                    SoapResponse& out);
    void dump(const char* what, const std::string& xml) const;

    platform::IHttpClient& http_;
    int timeoutMs_;
    std::string ip_;
    bool dumpXml_ = false;
};

} // namespace sonos
