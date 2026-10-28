#pragma once

#include <string>
#include <string_view>

namespace tinyxml2 {
class XMLElement;
}

namespace sonos {

// Element name without namespace prefix ("dc:title" -> "title").
std::string_view localName(const tinyxml2::XMLElement* element);

// First child element whose local name matches, or nullptr. element may be null.
const tinyxml2::XMLElement* findChild(const tinyxml2::XMLElement* element, std::string_view name);

// Decoded text of the first matching child, or "" if missing.
std::string childText(const tinyxml2::XMLElement* element, std::string_view name);

} // namespace sonos
