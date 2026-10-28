#include "sonos/XmlUtil.h"

#include <tinyxml2.h>

namespace sonos {

std::string_view localName(const tinyxml2::XMLElement* element) {
    std::string_view name = element->Name();
    const auto colon = name.find(':');
    return colon == std::string_view::npos ? name : name.substr(colon + 1);
}

const tinyxml2::XMLElement* findChild(const tinyxml2::XMLElement* element, std::string_view name) {
    if (!element) return nullptr;
    for (const tinyxml2::XMLElement* child = element->FirstChildElement(); child;
         child = child->NextSiblingElement()) {
        if (localName(child) == name) return child;
    }
    return nullptr;
}

std::string childText(const tinyxml2::XMLElement* element, std::string_view name) {
    const tinyxml2::XMLElement* child = findChild(element, name);
    const char* text = child ? child->GetText() : nullptr;
    return text ? text : "";
}

} // namespace sonos
