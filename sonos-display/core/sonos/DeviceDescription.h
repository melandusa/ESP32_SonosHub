#pragma once

#include <string>

namespace sonos {

struct DeviceInfo {
    std::string roomName;
    std::string modelName;
    bool isPlayer = false;  // has a MediaRenderer (false for Bridge/Boost)
};

// Parses /xml/device_description.xml. Returns false if the XML is not a device description.
bool parseDeviceDescription(const std::string& xml, DeviceInfo& out);

} // namespace sonos
