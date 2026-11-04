#include "sonos/SonosClient.h"

#include "sonos/LogoUrl.h"

#include "platform/Log.h"

#include <cstdlib>

namespace sonos {

const char* callStatusName(CallStatus status) {
    switch (status) {
    case CallStatus::Ok: return "ok";
    case CallStatus::NetworkError: return "network error";
    case CallStatus::SoapFault: return "SOAP fault";
    case CallStatus::ParseError: return "parse error";
    }
    return "?";
}

SonosClient::SonosClient(platform::IHttpClient& http, int timeoutMs)
    : http_(http), timeoutMs_(timeoutMs) {}

std::string SonosClient::baseUrl() const {
    return "http://" + ip_ + ":" + std::to_string(kSonosPort);
}

void SonosClient::dump(const char* what, const std::string& xml) const {
    if (dumpXml_) LOGI("--- XML %s (%zu bytes) ---\n%s", what, xml.size(), xml.c_str());
}

CallStatus SonosClient::call(const ServiceInfo& service, const char* action, const SoapArgs& args,
                             SoapResponse& out) {
    if (ip_.empty()) return CallStatus::NetworkError;

    const platform::HttpHeaders headers = {
        {"Content-Type", "text/xml; charset=\"utf-8\""},
        {"SOAPACTION", buildSoapActionHeader(service.serviceType, action)},
    };
    const std::string body = buildSoapEnvelope(service.serviceType, action, args);
    const platform::HttpResponse response =
        http_.post(baseUrl() + service.controlPath, headers, body, timeoutMs_);

    if (response.status == 0) {
        LOGD("%s: no response from %s", action, ip_.c_str());
        return CallStatus::NetworkError;
    }
    dump(action, response.body);

    out = parseSoapResponse(response.body);
    if (out.fault) {
        LOGW("%s: SOAP fault, UPnP error %s", action, out.upnpErrorCode.c_str());
        return CallStatus::SoapFault;
    }
    if (response.status != 200 || !out.ok) {
        LOGW("%s: unexpected response (HTTP %d)", action, response.status);
        return CallStatus::ParseError;
    }
    return CallStatus::Ok;
}

CallStatus SonosClient::fetchDeviceInfo(const std::string& ip, DeviceInfo& out) {
    const std::string url = "http://" + ip + ":" + std::to_string(kSonosPort) +
                            "/xml/device_description.xml";
    const platform::HttpResponse response = http_.get(url, timeoutMs_);
    if (response.status == 0) return CallStatus::NetworkError;
    dump("device_description", response.body);
    if (response.status != 200 || !parseDeviceDescription(response.body, out)) {
        return CallStatus::ParseError;
    }
    return CallStatus::Ok;
}

CallStatus SonosClient::fetchAlbumArt(const std::string& streamUri, std::size_t maxBytes,
                                      std::string& out) {
    if (ip_.empty()) return CallStatus::NetworkError;
    // Images take longer than SOAP calls; the speaker fetches them from the internet first.
    platform::HttpResponse response = http_.get(albumArtProxyUrl(baseUrl(), streamUri), 3 * timeoutMs_);
    if (response.status == 0) return CallStatus::NetworkError;
    if (response.status != 200 || response.body.empty() || response.body.size() > maxBytes) {
        LOGD("Album art: HTTP %d, %zu bytes", response.status, response.body.size());
        return CallStatus::ParseError;
    }
    out = std::move(response.body);
    return CallStatus::Ok;
}

CallStatus SonosClient::browseFavorites(FavoritesParseResult& out) {
    SoapResponse response;
    const CallStatus status = call(kContentDirectory, "Browse",
                                   {{"ObjectID", "FV:2"},
                                    {"BrowseFlag", "BrowseDirectChildren"},
                                    {"Filter", "dc:title,res,upnp:albumArtURI"},
                                    {"StartingIndex", "0"},
                                    {"RequestedCount", "100"},
                                    {"SortCriteria", ""}},
                                   response);
    if (status != CallStatus::Ok) return status;

    // <Result> holds escaped DIDL-Lite; tinyxml2 has decoded it once already.
    out = parseFavoritesDidl(response.values["Result"], baseUrl());
    return out.ok ? CallStatus::Ok : CallStatus::ParseError;
}

CallStatus SonosClient::setAVTransportUri(const std::string& uri, const std::string& metadata) {
    SoapResponse response;
    return call(kAVTransport, "SetAVTransportURI",
                {{"InstanceID", "0"}, {"CurrentURI", uri}, {"CurrentURIMetaData", metadata}},
                response);
}

CallStatus SonosClient::play() {
    SoapResponse response;
    return call(kAVTransport, "Play", {{"InstanceID", "0"}, {"Speed", "1"}}, response);
}

CallStatus SonosClient::stop() {
    SoapResponse response;
    return call(kAVTransport, "Stop", {{"InstanceID", "0"}}, response);
}

CallStatus SonosClient::getTransportInfo(TransportState& out) {
    SoapResponse response;
    const CallStatus status = call(kAVTransport, "GetTransportInfo", {{"InstanceID", "0"}}, response);
    if (status != CallStatus::Ok) return status;
    out = parseTransportState(response.values["CurrentTransportState"]);
    return CallStatus::Ok;
}

CallStatus SonosClient::getMediaInfo(MediaInfo& out) {
    SoapResponse response;
    const CallStatus status = call(kAVTransport, "GetMediaInfo", {{"InstanceID", "0"}}, response);
    if (status != CallStatus::Ok) return status;
    out.uri = response.values["CurrentURI"];
    out.title = parseDidlTitle(response.values["CurrentURIMetaData"]);
    return CallStatus::Ok;
}

CallStatus SonosClient::getVolume(int& out) {
    SoapResponse response;
    const CallStatus status = call(kRenderingControl, "GetVolume",
                                   {{"InstanceID", "0"}, {"Channel", "Master"}}, response);
    if (status != CallStatus::Ok) return status;
    const std::string& value = response.values["CurrentVolume"];
    if (value.empty()) return CallStatus::ParseError;
    out = std::atoi(value.c_str());
    return CallStatus::Ok;
}

CallStatus SonosClient::setVolume(int volume) {
    SoapResponse response;
    return call(kRenderingControl, "SetVolume",
                {{"InstanceID", "0"}, {"Channel", "Master"}, {"DesiredVolume", std::to_string(volume)}},
                response);
}

} // namespace sonos
