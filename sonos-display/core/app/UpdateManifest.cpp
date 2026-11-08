#include "app/UpdateManifest.h"

#include <cctype>

namespace app {

namespace {

// Position just after `"key"` and the following colon, npos if missing.
std::string::size_type valueStart(const std::string& json, const char* key) {
    const std::string quoted = std::string("\"") + key + "\"";
    auto pos = json.find(quoted);
    if (pos == std::string::npos) return pos;
    pos = json.find_first_not_of(" \t\r\n", pos + quoted.size());
    if (pos == std::string::npos || json[pos] != ':') return std::string::npos;
    return json.find_first_not_of(" \t\r\n", pos + 1);
}

} // namespace

bool parseUpdateManifest(const std::string& json, UpdateManifest& out) {
    auto pos = valueStart(json, "version");
    if (pos == std::string::npos) return false;
    long version = 0;
    bool digits = false;
    while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos])) && version < 1000000) {
        version = version * 10 + (json[pos++] - '0');
        digits = true;
    }
    if (!digits || version <= 0) return false;

    pos = valueStart(json, "url");
    if (pos == std::string::npos || json[pos] != '"') return false;
    std::string url;
    for (++pos; pos < json.size() && json[pos] != '"'; ++pos) {
        if (json[pos] == '\\' && pos + 1 < json.size()) ++pos;  // \/ and \" as written by JSON tools
        url += json[pos];
    }
    if (pos >= json.size() || url.rfind("https://", 0) != 0) return false;

    out.version = static_cast<int>(version);
    out.url = std::move(url);
    return true;
}

} // namespace app
