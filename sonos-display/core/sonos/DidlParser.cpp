#include "sonos/DidlParser.h"

#include "sonos/XmlUtil.h"

#include <tinyxml2.h>

namespace sonos {

namespace {

bool startsWith(const std::string& text, const char* prefix) {
    return text.rfind(prefix, 0) == 0;
}

std::string makeAbsolute(const std::string& url, const std::string& baseUrl) {
    if (url.empty() || startsWith(url, "http://") || startsWith(url, "https://")) return url;
    if (url.front() == '/') return baseUrl + url;
    return baseUrl + "/" + url;
}

} // namespace

bool isRadioUri(const std::string& uri) {
    static const char* const kRadioPrefixes[] = {
        "x-sonosapi-stream:", "x-sonosapi-radio:", "x-rincon-mp3radio:", "aac:", "hls-radio:",
    };
    for (const char* prefix : kRadioPrefixes) {
        if (startsWith(uri, prefix)) return true;
    }
    return false;
}

FavoritesParseResult parseFavoritesDidl(const std::string& didl, const std::string& baseUrl) {
    FavoritesParseResult result;
    tinyxml2::XMLDocument doc;
    if (doc.Parse(didl.c_str(), didl.size()) != tinyxml2::XML_SUCCESS) return result;
    const tinyxml2::XMLElement* root = doc.RootElement();
    if (!root || localName(root) != "DIDL-Lite") return result;

    for (const tinyxml2::XMLElement* item = root->FirstChildElement(); item;
         item = item->NextSiblingElement()) {
        if (localName(item) != "item") continue;

        Favorite fav;
        fav.title = childText(item, "title");
        fav.uri = childText(item, "res");
        // resMD is DIDL-Lite escaped once more; tinyxml2 has decoded one level here,
        // which is exactly what SetAVTransportURI expects as CurrentURIMetaData.
        fav.metadata = childText(item, "resMD");
        fav.albumArtUri = makeAbsolute(childText(item, "albumArtURI"), baseUrl);

        if (isRadioUri(fav.uri)) {
            result.radio.push_back(std::move(fav));
        } else {
            result.skippedTitles.push_back(fav.title);
        }
    }
    result.ok = true;
    return result;
}

std::string parseDidlTitle(const std::string& didl) {
    if (didl.empty()) return {};
    tinyxml2::XMLDocument doc;
    if (doc.Parse(didl.c_str(), didl.size()) != tinyxml2::XML_SUCCESS) return {};
    const tinyxml2::XMLElement* root = doc.RootElement();
    if (!root) return {};
    for (const tinyxml2::XMLElement* item = root->FirstChildElement(); item;
         item = item->NextSiblingElement()) {
        if (localName(item) == "item" || localName(item) == "container") {
            return childText(item, "title");
        }
    }
    return {};
}

} // namespace sonos
