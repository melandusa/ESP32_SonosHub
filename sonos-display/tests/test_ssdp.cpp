#include "sonos/Ssdp.h"

#include <doctest/doctest.h>

using sonos::parseSsdpResponse;

TEST_CASE("SSDP: IP from the LOCATION header of a ZonePlayer response") {
    const std::string response =
        "HTTP/1.1 200 OK\r\n"
        "CACHE-CONTROL: max-age = 1800\r\n"
        "EXT:\r\n"
        "LOCATION: http://192.168.1.50:1400/xml/device_description.xml\r\n"
        "SERVER: Linux UPnP/1.0 Sonos/80.1-55014 (ZPS18)\r\n"
        "ST: urn:schemas-upnp-org:device:ZonePlayer:1\r\n"
        "USN: uuid:RINCON_000000000000001400::urn:schemas-upnp-org:device:ZonePlayer:1\r\n"
        "X-RINCON-HOUSEHOLD: Sonos_abc\r\n\r\n";
    CHECK(parseSsdpResponse(response) == "192.168.1.50");
}

TEST_CASE("SSDP: header names are case-insensitive") {
    const std::string response =
        "HTTP/1.1 200 OK\r\n"
        "Location: http://10.0.0.7:1400/xml/device_description.xml\r\n"
        "st: urn:schemas-upnp-org:device:ZonePlayer:1\r\n\r\n";
    CHECK(parseSsdpResponse(response) == "10.0.0.7");
}

TEST_CASE("SSDP: other devices are ignored") {
    const std::string router =
        "HTTP/1.1 200 OK\r\n"
        "LOCATION: http://192.168.1.1:49000/igddesc.xml\r\n"
        "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n";
    CHECK(parseSsdpResponse(router).empty());
    CHECK(parseSsdpResponse("").empty());
}
