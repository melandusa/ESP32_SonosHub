#include "app/UpdateManifest.h"

#include <doctest/doctest.h>

using app::UpdateManifest;
using app::parseUpdateManifest;

TEST_CASE("Update manifest: as written by the release workflow") {
    UpdateManifest manifest;
    REQUIRE(parseUpdateManifest(
        "{\"version\": 3, \"url\": \"https://github.com/o/r/releases/download/v3/firmware.bin\"}\n", manifest));
    CHECK(manifest.version == 3);
    CHECK(manifest.url == "https://github.com/o/r/releases/download/v3/firmware.bin");
}

TEST_CASE("Update manifest: whitespace, key order and escaped slashes") {
    UpdateManifest manifest;
    REQUIRE(parseUpdateManifest("{\n  \"url\" : \"https:\\/\\/example.org\\/fw.bin\",\n  \"version\" : 12\n}",
                                manifest));
    CHECK(manifest.version == 12);
    CHECK(manifest.url == "https://example.org/fw.bin");
}

TEST_CASE("Update manifest: rejects incomplete or unsafe data") {
    UpdateManifest manifest;
    CHECK_FALSE(parseUpdateManifest("", manifest));
    CHECK_FALSE(parseUpdateManifest("<html>Not Found</html>", manifest));
    CHECK_FALSE(parseUpdateManifest("{\"version\": 0, \"url\": \"https://x/fw.bin\"}", manifest));
    CHECK_FALSE(parseUpdateManifest("{\"version\": \"3\", \"url\": \"https://x/fw.bin\"}", manifest));
    CHECK_FALSE(parseUpdateManifest("{\"version\": 3, \"url\": \"http://x/fw.bin\"}", manifest));
    CHECK_FALSE(parseUpdateManifest("{\"version\": 3}", manifest));
    CHECK_FALSE(parseUpdateManifest("{\"version\": 3, \"url\": \"https://x/fw.bin", manifest));
}
