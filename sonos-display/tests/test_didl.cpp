#include "TestUtil.h"
#include "sonos/DeviceDescription.h"
#include "sonos/DidlParser.h"
#include "sonos/SoapParser.h"

#include <doctest/doctest.h>

using namespace sonos;

namespace {

FavoritesParseResult parseFixture() {
    SoapResponse response = parseSoapResponse(readFixture("browse_favorites.xml"));
    REQUIRE(response.ok);
    return parseFavoritesDidl(response.values["Result"], "http://192.168.1.50:1400");
}

} // namespace

TEST_CASE("Browse result: only radio favorites, in Sonos order") {
    FavoritesParseResult result = parseFixture();
    REQUIRE(result.ok);
    REQUIRE(result.radio.size() == 3);
    CHECK(result.radio[0].title == "SWR1 Baden-Württemberg");
    CHECK(result.radio[1].title == "Deutschlandfunk");
    CHECK(result.radio[2].title == "ANTENNE BAYERN");
}

TEST_CASE("Browse result: playlists and tracks are skipped") {
    FavoritesParseResult result = parseFixture();
    REQUIRE(result.skippedTitles.size() == 2);
    CHECK(result.skippedTitles[0] == "Lieblingslieder");
    CHECK(result.skippedTitles[1] == "Bohemian Rhapsody");
}

TEST_CASE("Browse result: res is fully decoded") {
    FavoritesParseResult result = parseFixture();
    REQUIRE(result.radio.size() == 3);
    CHECK(result.radio[0].uri == "x-sonosapi-stream:s20291?sid=254&flags=8224&sn=0");
    CHECK(result.radio[1].uri == "x-rincon-mp3radio://st01.sslstream.dlf.de/dlf/01/128/mp3/stream.mp3");
}

TEST_CASE("Browse result: double escaped resMD is decoded to plain DIDL once") {
    FavoritesParseResult result = parseFixture();
    REQUIRE(result.radio.size() == 3);
    const std::string& metadata = result.radio[0].metadata;
    CHECK(metadata.rfind("<DIDL-Lite ", 0) == 0);
    CHECK(metadata.find("&lt;") == std::string::npos);
    CHECK(metadata.find("<desc id=\"cdudn\"") != std::string::npos);
    CHECK(metadata.find("SA_RINCON65031_") != std::string::npos);
    // The metadata itself is valid DIDL again
    CHECK(parseDidlTitle(metadata) == "SWR1 Baden-Württemberg");
}

TEST_CASE("Browse result: album art paths become absolute") {
    FavoritesParseResult result = parseFixture();
    REQUIRE(result.radio.size() == 3);
    CHECK(result.radio[0].albumArtUri ==
          "http://192.168.1.50:1400/getaa?s=1&u=x-sonosapi-stream%3as20291%3fsid%3d254%26flags%3d8224%26sn%3d0");
    CHECK(result.radio[1].albumArtUri.rfind("https://www.deutschlandfunk.de/", 0) == 0);
}

TEST_CASE("Radio URI detection") {
    CHECK(isRadioUri("x-sonosapi-stream:s20291?sid=254"));
    CHECK(isRadioUri("x-sonosapi-radio:ST%3aabc?sid=236"));
    CHECK(isRadioUri("x-rincon-mp3radio://example.com/stream.mp3"));
    CHECK(isRadioUri("aac://https://example.com/stream"));
    CHECK(isRadioUri("hls-radio://example.com/live.m3u8"));
    CHECK_FALSE(isRadioUri("x-rincon-cpcontainer:1006206c"));
    CHECK_FALSE(isRadioUri("x-sonos-spotify:spotify%3atrack"));
    CHECK_FALSE(isRadioUri(""));
}

TEST_CASE("Media info: station title from CurrentURIMetaData") {
    SoapResponse response = parseSoapResponse(readFixture("get_media_info.xml"));
    REQUIRE(response.ok);
    CHECK(response.values["CurrentURI"] == "x-sonosapi-stream:s20291?sid=254&flags=8224&sn=0");
    CHECK(parseDidlTitle(response.values["CurrentURIMetaData"]) == "SWR1 Baden-Württemberg");
}

TEST_CASE("DIDL edge cases") {
    CHECK(parseDidlTitle("") == "");
    CHECK(parseDidlTitle("NOT_IMPLEMENTED") == "");
    CHECK_FALSE(parseFavoritesDidl("", "http://x:1400").ok);
    FavoritesParseResult empty = parseFavoritesDidl("<DIDL-Lite></DIDL-Lite>", "http://x:1400");
    CHECK(empty.ok);
    CHECK(empty.radio.empty());
}

TEST_CASE("Device description") {
    DeviceInfo info;
    REQUIRE(parseDeviceDescription(readFixture("device_description.xml"), info));
    CHECK(info.roomName == "Küche");
    CHECK(info.modelName == "Sonos One");
    CHECK(info.isPlayer);

    DeviceInfo bridge;
    REQUIRE(parseDeviceDescription(
        "<root><device><roomName>BRIDGE</roomName><deviceList><device><deviceType>"
        "urn:schemas-upnp-org:device:MediaServer:1</deviceType></device></deviceList></device></root>",
        bridge));
    CHECK_FALSE(bridge.isPlayer);
}

// --- Responses recorded from a real Sonos Ray (tests/fixtures/real) ---

TEST_CASE("Real Browse response: radio favorites and hidden shortcuts") {
    SoapResponse response = parseSoapResponse(readFixture("real/browse_favorites.xml"));
    REQUIRE(response.ok);
    FavoritesParseResult result = parseFavoritesDidl(response.values["Result"], "http://192.168.178.106:1400");
    REQUIRE(result.ok);
    REQUIRE(result.radio.size() == 4);
    CHECK(result.radio[0].title == "Absolut relax");
    CHECK(result.radio[1].title == "Feeling Happy - Gute-Laune-Radio");
    CHECK(result.radio[2].title == "SWR1 Baden-Württemberg");
    CHECK(result.radio[3].title == "SWR3");
    CHECK(result.radio[3].uri == "x-sonosapi-stream:tunein%3a9479?sid=303&flags=8224&sn=2");
    // Sonos Radio shortcuts come without <res>
    CHECK(result.skippedTitles.size() == 3);
    for (const Favorite& fav : result.radio) {
        CHECK(fav.metadata.rfind("<DIDL-Lite", 0) == 0);
        CHECK(parseDidlTitle(fav.metadata) == fav.title);
    }
}

TEST_CASE("Real GetMediaInfo: playing URI differs from the favorite only in its flags") {
    SoapResponse response = parseSoapResponse(readFixture("real/get_media_info.xml"));
    REQUIRE(response.ok);
    const std::string& playing = response.values["CurrentURI"];
    CHECK(playing == "x-sonosapi-stream:tunein%3a9479?sid=303&flags=8232&sn=2");
    CHECK(parseDidlTitle(response.values["CurrentURIMetaData"]) == "SWR3");
    CHECK(isSameStream(playing, "x-sonosapi-stream:tunein%3a9479?sid=303&flags=8224&sn=2"));
}

TEST_CASE("Real device description of a Sonos Ray") {
    DeviceInfo info;
    REQUIRE(parseDeviceDescription(readFixture("real/device_description.xml"), info));
    CHECK(info.roomName == "Wohnzimmer");
    CHECK(info.modelName == "Sonos Ray");
    CHECK(info.isPlayer);
}

TEST_CASE("isSameStream") {
    CHECK(isSameStream("x-sonosapi-stream:tunein%3A7487?sid=303&flags=8292", "x-sonosapi-stream:tunein%3a7487?sid=303"));
    CHECK(isSameStream("aac://http://a/b", "aac://http://a/b"));
    CHECK_FALSE(isSameStream("x-sonosapi-stream:tunein%3a7487", "x-sonosapi-stream:tunein%3a9479"));
    CHECK_FALSE(isSameStream("", ""));
}

TEST_CASE("mediaSourceOf: speaker inputs") {
    CHECK(mediaSourceOf("x-sonos-htastream:RINCON_000E5800000101400:spdif") == MediaSource::TvInput);
    CHECK(mediaSourceOf("x-rincon-stream:RINCON_000E5800000101400") == MediaSource::LineIn);
    CHECK(mediaSourceOf("x-sonosapi-stream:tunein%3a9479?sid=303") == MediaSource::Other);
    CHECK(mediaSourceOf("") == MediaSource::Other);
}
