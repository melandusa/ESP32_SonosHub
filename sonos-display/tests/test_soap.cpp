#include "TestUtil.h"
#include "sonos/SoapBuilder.h"
#include "sonos/SoapParser.h"
#include "sonos/Types.h"

#include <doctest/doctest.h>

using namespace sonos;

TEST_CASE("escapeXml escapes the five XML special characters") {
    CHECK(escapeXml("a&b<c>d\"e'f") == "a&amp;b&lt;c&gt;d&quot;e&apos;f");
    CHECK(escapeXml("Württemberg") == "Württemberg");
    CHECK(escapeXml("") == "");
}

TEST_CASE("SOAPACTION header has service type, action and quotes") {
    CHECK(buildSoapActionHeader("urn:schemas-upnp-org:service:AVTransport:1", "Play") ==
          "\"urn:schemas-upnp-org:service:AVTransport:1#Play\"");
}

TEST_CASE("SetVolume envelope matches the Sonos format") {
    const std::string body = buildSoapEnvelope("urn:schemas-upnp-org:service:RenderingControl:1",
                                               "SetVolume",
                                               {{"InstanceID", "0"}, {"Channel", "Master"}, {"DesiredVolume", "35"}});
    CHECK(body ==
          "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
          "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
          " s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
          "<s:Body><u:SetVolume xmlns:u=\"urn:schemas-upnp-org:service:RenderingControl:1\">"
          "<InstanceID>0</InstanceID><Channel>Master</Channel><DesiredVolume>35</DesiredVolume>"
          "</u:SetVolume></s:Body></s:Envelope>");
}

TEST_CASE("Argument values are escaped and survive a parse round trip") {
    const std::string uri = "x-sonosapi-stream:s20291?sid=254&flags=8224&sn=0";
    const std::string metadata = "<DIDL-Lite><item><dc:title>Rock & Pop</dc:title></item></DIDL-Lite>";
    const std::string body = buildSoapEnvelope("urn:schemas-upnp-org:service:AVTransport:1",
                                               "SetAVTransportURI",
                                               {{"InstanceID", "0"}, {"CurrentURI", uri}, {"CurrentURIMetaData", metadata}});

    CHECK(body.find("<CurrentURI>x-sonosapi-stream:s20291?sid=254&amp;flags=8224&amp;sn=0</CurrentURI>") !=
          std::string::npos);
    CHECK(body.find("&lt;DIDL-Lite&gt;&lt;item&gt;&lt;dc:title&gt;Rock &amp; Pop") != std::string::npos);

    // A request has the same Body/<Action>/<args> shape as a response
    SoapResponse parsed = parseSoapResponse(body);
    REQUIRE(parsed.ok);
    CHECK(parsed.values["CurrentURI"] == uri);
    CHECK(parsed.values["CurrentURIMetaData"] == metadata);
}

TEST_CASE("Empty arguments produce empty elements") {
    const std::string body = buildSoapEnvelope("svc", "Browse", {{"SortCriteria", ""}});
    CHECK(body.find("<SortCriteria></SortCriteria>") != std::string::npos);
}

TEST_CASE("GetVolume response") {
    SoapResponse response = parseSoapResponse(readFixture("get_volume.xml"));
    REQUIRE(response.ok);
    CHECK_FALSE(response.fault);
    CHECK(response.values["CurrentVolume"] == "23");
}

TEST_CASE("GetTransportInfo response") {
    SoapResponse response = parseSoapResponse(readFixture("get_transport_info.xml"));
    REQUIRE(response.ok);
    CHECK(response.values["CurrentTransportState"] == "PLAYING");
    CHECK(parseTransportState(response.values["CurrentTransportState"]) == TransportState::Playing);
}

TEST_CASE("SOAP fault with UPnP error code") {
    SoapResponse response = parseSoapResponse(readFixture("soap_fault.xml"));
    CHECK_FALSE(response.ok);
    CHECK(response.fault);
    CHECK(response.upnpErrorCode == "701");
}

TEST_CASE("Malformed or unexpected bodies are rejected") {
    CHECK_FALSE(parseSoapResponse("").ok);
    CHECK_FALSE(parseSoapResponse("<html><body>Error</body></html>").ok);
    CHECK_FALSE(parseSoapResponse("<s:Envelope><s:Body>").ok);
}

TEST_CASE("Transport states") {
    CHECK(parseTransportState("STOPPED") == TransportState::Stopped);
    CHECK(parseTransportState("PAUSED_PLAYBACK") == TransportState::Paused);
    CHECK(parseTransportState("TRANSITIONING") == TransportState::Transitioning);
    CHECK(parseTransportState("") == TransportState::Unknown);
}
