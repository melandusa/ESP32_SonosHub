#include "sonos/SoapParser.h"

#include "sonos/XmlUtil.h"

#include <tinyxml2.h>

namespace sonos {

SoapResponse parseSoapResponse(const std::string& body) {
    SoapResponse response;
    tinyxml2::XMLDocument doc;
    if (doc.Parse(body.c_str(), body.size()) != tinyxml2::XML_SUCCESS) return response;

    const tinyxml2::XMLElement* envelope = doc.RootElement();
    const tinyxml2::XMLElement* soapBody = findChild(envelope, "Body");
    if (!soapBody) return response;

    const tinyxml2::XMLElement* payload = soapBody->FirstChildElement();
    if (!payload) return response;

    if (localName(payload) == "Fault") {
        response.fault = true;
        // Fault/detail/UPnPError/errorCode
        const tinyxml2::XMLElement* error = findChild(findChild(payload, "detail"), "UPnPError");
        response.upnpErrorCode = childText(error, "errorCode");
        return response;
    }

    for (const tinyxml2::XMLElement* arg = payload->FirstChildElement(); arg;
         arg = arg->NextSiblingElement()) {
        const char* text = arg->GetText();
        response.values[std::string(localName(arg))] = text ? text : "";
    }
    response.ok = true;
    return response;
}

} // namespace sonos
