#include "TestUtil.h"
#include "sonos/SonosClient.h"

#include <doctest/doctest.h>

#include <map>

using namespace sonos;

namespace {

// Answers SOAP calls with fixtures and records the requests.
class FakeHttp : public platform::IHttpClient {
public:
    platform::HttpResponse post(const std::string& url, const platform::HttpHeaders& headers,
                                const std::string& body, int /*timeoutMs*/) override {
        lastUrl = url;
        lastBody = body;
        for (const auto& header : headers) lastHeaders[header.first] = header.second;
        if (offline) return {0, ""};
        return reply;
    }
    platform::HttpResponse get(const std::string& url, int /*timeoutMs*/) override {
        lastUrl = url;
        if (offline) return {0, ""};
        return reply;
    }

    platform::HttpResponse reply{200, ""};
    bool offline = false;
    std::string lastUrl;
    std::string lastBody;
    std::map<std::string, std::string> lastHeaders;
};

} // namespace

TEST_CASE("SonosClient: favorites from a Browse response") {
    FakeHttp http;
    http.reply.body = readFixture("browse_favorites.xml");
    SonosClient client(http);
    client.setDevice("192.168.1.50");

    FavoritesParseResult favorites;
    REQUIRE(client.browseFavorites(favorites) == CallStatus::Ok);
    CHECK(favorites.radio.size() == 3);
    CHECK(http.lastUrl == "http://192.168.1.50:1400/MediaServer/ContentDirectory/Control");
    CHECK(http.lastHeaders["SOAPACTION"] == "\"urn:schemas-upnp-org:service:ContentDirectory:1#Browse\"");
    CHECK(http.lastHeaders["Content-Type"] == "text/xml; charset=\"utf-8\"");
    CHECK(http.lastBody.find("<ObjectID>FV:2</ObjectID>") != std::string::npos);
    CHECK(http.lastBody.find("<RequestedCount>100</RequestedCount>") != std::string::npos);
}

TEST_CASE("SonosClient: favorite metadata is re-escaped exactly once for SetAVTransportURI") {
    FakeHttp http;
    http.reply.body = readFixture("browse_favorites.xml");
    SonosClient client(http);
    client.setDevice("192.168.1.50");
    FavoritesParseResult favorites;
    REQUIRE(client.browseFavorites(favorites) == CallStatus::Ok);
    REQUIRE_FALSE(favorites.radio.empty());

    http.reply.body = "<s:Envelope><s:Body><u:SetAVTransportURIResponse/></s:Body></s:Envelope>";
    REQUIRE(client.setAVTransportUri(favorites.radio[0].uri, favorites.radio[0].metadata) == CallStatus::Ok);
    CHECK(http.lastUrl == "http://192.168.1.50:1400/MediaRenderer/AVTransport/Control");
    CHECK(http.lastBody.find("<CurrentURIMetaData>&lt;DIDL-Lite xmlns:dc=&quot;") != std::string::npos);
    CHECK(http.lastBody.find("&amp;lt;") == std::string::npos);  // not double escaped
    CHECK(http.lastBody.find("<CurrentURI>x-sonosapi-stream:s20291?sid=254&amp;flags=8224&amp;sn=0</CurrentURI>") !=
          std::string::npos);
}

TEST_CASE("SonosClient: status calls") {
    FakeHttp http;
    SonosClient client(http);
    client.setDevice("10.0.0.2");

    http.reply.body = readFixture("get_volume.xml");
    int volume = 0;
    CHECK(client.getVolume(volume) == CallStatus::Ok);
    CHECK(volume == 23);
    CHECK(http.lastUrl == "http://10.0.0.2:1400/MediaRenderer/RenderingControl/Control");

    http.reply.body = readFixture("get_transport_info.xml");
    TransportState state = TransportState::Unknown;
    CHECK(client.getTransportInfo(state) == CallStatus::Ok);
    CHECK(state == TransportState::Playing);

    http.reply.body = readFixture("get_media_info.xml");
    MediaInfo media;
    CHECK(client.getMediaInfo(media) == CallStatus::Ok);
    CHECK(media.title == "SWR1 Baden-Württemberg");
}

TEST_CASE("SonosClient: error classification") {
    FakeHttp http;
    SonosClient client(http);

    // No device yet
    CHECK(client.stop() == CallStatus::NetworkError);

    client.setDevice("10.0.0.2");
    http.offline = true;
    CHECK(client.stop() == CallStatus::NetworkError);

    http.offline = false;
    http.reply = {500, readFixture("soap_fault.xml")};
    CHECK(client.stop() == CallStatus::SoapFault);

    http.reply = {200, "garbage"};
    CHECK(client.stop() == CallStatus::ParseError);
}

TEST_CASE("SonosClient: device description") {
    FakeHttp http;
    http.reply.body = readFixture("device_description.xml");
    SonosClient client(http);
    DeviceInfo info;
    CHECK(client.fetchDeviceInfo("192.168.1.50", info) == CallStatus::Ok);
    CHECK(http.lastUrl == "http://192.168.1.50:1400/xml/device_description.xml");
    CHECK(info.roomName == "Küche");
}
