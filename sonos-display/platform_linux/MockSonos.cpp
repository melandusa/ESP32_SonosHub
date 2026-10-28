#include "MockSonos.h"

#include "platform/Log.h"
#include "sonos/SoapBuilder.h"
#include "sonos/SoapParser.h"

#include <chrono>
#include <cstdlib>
#include <thread>

namespace linux_platform {

namespace {

constexpr const char* kMockIp = "127.0.0.1";

std::string envelope(const std::string& action, const char* serviceType, const std::string& inner) {
    return "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
           " s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:" +
           action + "Response xmlns:u=\"" + serviceType + "\">" + inner + "</u:" + action +
           "Response></s:Body></s:Envelope>";
}

std::string element(const char* name, const std::string& value) {
    return std::string("<") + name + ">" + sonos::escapeXml(value) + "</" + name + ">";
}

const char* serviceFor(const std::string& action) {
    if (action == "Browse") return "urn:schemas-upnp-org:service:ContentDirectory:1";
    if (action == "GetVolume" || action == "SetVolume")
        return "urn:schemas-upnp-org:service:RenderingControl:1";
    return "urn:schemas-upnp-org:service:AVTransport:1";
}

} // namespace

MockSonos::MockSonos(int latencyMs)
    : latencyMs_(latencyMs),
      stations_{
          {"SWR1 Baden-Württemberg", "x-sonosapi-stream:s20291?sid=254&flags=8224&sn=0"},
          {"SWR3", "x-sonosapi-stream:s24896?sid=254&flags=8224&sn=0"},
          {"ANTENNE BAYERN", "x-sonosapi-stream:s42824?sid=254&flags=8224&sn=0"},
          {"Deutschlandfunk", "x-rincon-mp3radio://st01.sslstream.dlf.de/dlf/01/128/mp3/stream.mp3"},
          {"hr1", "aac://https://dispatcher.rndfnk.com/hr/hr1/live/mp3/high"},
          {"WDR 4", "x-sonosapi-stream:s25217?sid=254&flags=8224&sn=0"},
          {"Bayern 1 Oberbayern", "x-sonosapi-stream:s14991?sid=254&flags=8224&sn=0"},
          {"NDR 2 Süd & Nord", "x-sonosapi-stream:s17492?sid=254&flags=8224&sn=0"},
      } {}

void MockSonos::simulateLatency() const {
    std::this_thread::sleep_for(std::chrono::milliseconds(latencyMs_));
}

std::vector<std::string> MockSonos::discover(int timeoutMs) {
    if (!online_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
        return {};
    }
    simulateLatency();
    return {kMockIp};
}

platform::HttpResponse MockSonos::get(const std::string& url, int timeoutMs) {
    if (!online_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
        return {0, ""};
    }
    simulateLatency();
    if (url.find("/xml/device_description.xml") == std::string::npos) return {404, ""};
    return {200,
            "<?xml version=\"1.0\"?><root xmlns=\"urn:schemas-upnp-org:device-1-0\"><device>"
            "<deviceType>urn:schemas-upnp-org:device:ZonePlayer:1</deviceType>"
            "<roomName>Wohnzimmer</roomName><modelName>Sonos One (Simulation)</modelName>"
            "<deviceList><device><deviceType>urn:schemas-upnp-org:device:MediaRenderer:1"
            "</deviceType></device></deviceList></device></root>"};
}

platform::HttpResponse MockSonos::post(const std::string& /*url*/,
                                       const platform::HttpHeaders& headers,
                                       const std::string& body, int timeoutMs) {
    if (!online_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
        return {0, ""};
    }
    simulateLatency();
    std::string soapAction;
    for (const auto& [name, value] : headers) {
        if (name == "SOAPACTION") soapAction = value;
    }
    const auto hash = soapAction.find('#');
    if (hash == std::string::npos) return {400, ""};
    const std::string action = soapAction.substr(hash + 1, soapAction.size() - hash - 2);

    std::lock_guard<std::mutex> lock(mutex_);
    return {200, handle(action, body)};
}

std::string MockSonos::handle(const std::string& action, const std::string& requestBody) {
    // A SOAP request has the same shape as a response: Body/<action>/<arguments>.
    sonos::SoapResponse request = sonos::parseSoapResponse(requestBody);
    const char* service = serviceFor(action);
    LOGD("Mock: %s", action.c_str());

    if (action == "Browse") return envelope(action, service, browseResponse());
    if (action == "SetAVTransportURI") {
        currentUri_ = request.values["CurrentURI"];
        currentMetadata_ = request.values["CurrentURIMetaData"];
        transport_ = sonos::TransportState::Stopped;
        return envelope(action, service, "");
    }
    if (action == "Play") {
        transport_ = sonos::TransportState::Playing;
        return envelope(action, service, "");
    }
    if (action == "Stop") {
        transport_ = sonos::TransportState::Stopped;
        return envelope(action, service, "");
    }
    if (action == "GetTransportInfo") {
        return envelope(action, service,
                        element("CurrentTransportState", sonos::transportStateName(transport_)) +
                            element("CurrentTransportStatus", "OK") + element("CurrentSpeed", "1"));
    }
    if (action == "GetMediaInfo") {
        return envelope(action, service,
                        element("NrTracks", currentUri_.empty() ? "0" : "1") +
                            element("CurrentURI", currentUri_) +
                            element("CurrentURIMetaData", currentMetadata_));
    }
    if (action == "GetVolume") {
        return envelope(action, service, element("CurrentVolume", std::to_string(volume_)));
    }
    if (action == "SetVolume") {
        volume_ = std::atoi(request.values["DesiredVolume"].c_str());
        LOGI("Mock: volume set to %d", volume_);
        return envelope(action, service, "");
    }
    return envelope(action, service, "");
}

std::string MockSonos::itemMetadata(const Station& station) const {
    return "<DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
           "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" "
           "xmlns:r=\"urn:schemas-rinconnetworks-com:metadata-1-0/\" "
           "xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\">"
           "<item id=\"F00092020s0\" parentID=\"F00082064\" restricted=\"true\">" +
           element("dc:title", station.title) +
           "<upnp:class>object.item.audioItem.audioBroadcast</upnp:class>"
           "<desc id=\"cdudn\" nameSpace=\"urn:schemas-rinconnetworks-com:metadata-1-0/\">"
           "SA_RINCON65031_</desc></item></DIDL-Lite>";
}

std::string MockSonos::browseResponse() const {
    std::string didl =
        "<DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
        "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" "
        "xmlns:r=\"urn:schemas-rinconnetworks-com:metadata-1-0/\" "
        "xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\">";
    int index = 0;
    for (const Station& station : stations_) {
        didl += "<item id=\"FV:2/" + std::to_string(++index) + "\" parentID=\"FV:2\" restricted=\"false\">" +
                element("dc:title", station.title) +
                "<upnp:class>object.itemobject.item.sonos-favorite</upnp:class>"
                "<res protocolInfo=\"x-sonosapi-stream:*:*:*\">" + sonos::escapeXml(station.uri) + "</res>" +
                element("r:resMD", itemMetadata(station)) + "</item>";
    }
    // A playlist favorite, which the MVP must hide
    didl += "<item id=\"FV:2/99\" parentID=\"FV:2\" restricted=\"false\">"
            "<dc:title>Lieblingslieder</dc:title>"
            "<res protocolInfo=\"x-rincon-cpcontainer:*:*:*\">x-rincon-cpcontainer:1006206cplaylist</res>"
            "</item></DIDL-Lite>";

    const std::string count = std::to_string(stations_.size() + 1);
    return element("Result", didl) + element("NumberReturned", count) +
           element("TotalMatches", count) + element("UpdateID", "1");
}

} // namespace linux_platform
