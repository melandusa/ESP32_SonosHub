#include "TestUtil.h"
#include "sonos/LogoUrl.h"
#include "ui/LogoDecoder.h"

#include "lvgl.h"

#include <doctest/doctest.h>

namespace {

// Pixel in LVGL ARGB8888 byte order: B, G, R, A
struct Pixel {
    int b, g, r, a;
};

Pixel pixelAt(const app::LogoImage& logo, int x, int y) {
    const uint8_t* p = &logo.pixels[(static_cast<size_t>(y) * logo.size + x) * 4];
    return {p[0], p[1], p[2], p[3]};
}

void initLvgl() {
    if (!lv_is_initialized()) lv_init();  // lodepng allocates LVGL draw buffers
}

} // namespace

TEST_CASE("Logo URL: speaker art proxy with encoded stream URI") {
    CHECK(sonos::percentEncode("x-sonosapi-stream:tunein%3a34098?sid=303&flags=8232") ==
          "x-sonosapi-stream%3Atunein%253a34098%3Fsid%3D303%26flags%3D8232");
    CHECK(sonos::albumArtProxyUrl("http://192.168.1.50:1400", "aac://a b") ==
          "http://192.168.1.50:1400/getaa?s=1&u=aac%3A%2F%2Fa%20b");
}

// Fixture: 4x2 RGBA, left half opaque red, right half fully transparent.
TEST_CASE("Logo decoder: fits into the square, centered, alpha kept") {
    initLvgl();
    app::LogoImage logo;
    REQUIRE(ui::decodeLogo(readFixture("logo_4x2.png"), 4, logo));
    REQUIRE(logo.size == 4);
    REQUIRE(logo.pixels.size() == 4 * 4 * 4);

    // 4x2 is centered vertically: rows 1 and 2 hold the image
    CHECK(pixelAt(logo, 0, 0).a == 0);
    const Pixel red = pixelAt(logo, 0, 1);
    CHECK(red.r == 255);
    CHECK(red.g == 0);
    CHECK(red.b == 0);
    CHECK(red.a == 255);
    CHECK(pixelAt(logo, 3, 2).a == 0);
    CHECK(pixelAt(logo, 1, 3).a == 0);
}

TEST_CASE("Logo decoder: downscaling averages, transparent pixels do not darken") {
    initLvgl();
    app::LogoImage logo;
    REQUIRE(ui::decodeLogo(readFixture("logo_4x2.png"), 2, logo));
    REQUIRE(logo.size == 2);
    // 4x2 -> 2x1 in row 0
    const Pixel left = pixelAt(logo, 0, 0);
    CHECK(left.r == 255);
    CHECK(left.a == 255);
    CHECK(pixelAt(logo, 1, 0).a == 0);
    CHECK(pixelAt(logo, 0, 1).a == 0);
}

TEST_CASE("Logo decoder: rejects non-PNG data") {
    initLvgl();
    app::LogoImage logo;
    CHECK_FALSE(ui::decodeLogo("\xFF\xD8\xFF\xE0 not a png, a JPEG header", 88, logo));
    CHECK_FALSE(ui::decodeLogo("", 88, logo));
}
