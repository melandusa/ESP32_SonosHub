#pragma once

#include <map>
#include <string>

namespace sonos {

struct SoapResponse {
    bool ok = false;               // well-formed <...Response> element found
    bool fault = false;            // <s:Fault> present
    std::string upnpErrorCode;     // from <UPnPError><errorCode>, if any
    std::map<std::string, std::string> values;  // decoded text of the response arguments
};

// Parses a SOAP response body. Element names are matched without namespace prefix.
SoapResponse parseSoapResponse(const std::string& body);

} // namespace sonos
