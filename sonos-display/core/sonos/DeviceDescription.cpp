#include "sonos/DeviceDescription.h"

#include "sonos/XmlUtil.h"

#include <tinyxml2.h>

namespace sonos {

namespace {

bool hasMediaRenderer(const tinyxml2::XMLElement* device) {
    const tinyxml2::XMLElement* list = findChild(device, "deviceList");
    if (!list) return false;
    for (const tinyxml2::XMLElement* sub = list->FirstChildElement(); sub;
         sub = sub->NextSiblingElement()) {
        if (childText(sub, "deviceType").find(":MediaRenderer:") != std::string::npos) return true;
        if (hasMediaRenderer(sub)) return true;
    }
    return false;
}

} // namespace

bool parseDeviceDescription(const std::string& xml, DeviceInfo& out) {
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) return false;
    const tinyxml2::XMLElement* device = findChild(doc.RootElement(), "device");
    if (!device) return false;
    out.roomName = childText(device, "roomName");
    out.modelName = childText(device, "modelName");
    out.isPlayer = hasMediaRenderer(device);
    return true;
}

} // namespace sonos
