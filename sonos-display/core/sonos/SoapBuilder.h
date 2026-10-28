#pragma once

#include <string>
#include <utility>
#include <vector>

namespace sonos {

using SoapArgs = std::vector<std::pair<std::string, std::string>>;

// Escapes & < > " ' for use in XML text and attribute values.
std::string escapeXml(const std::string& text);

// Builds the complete SOAP envelope; argument values are escaped here.
std::string buildSoapEnvelope(const char* serviceType, const char* action, const SoapArgs& args);

// Value of the SOAPACTION header, including the surrounding quotes.
std::string buildSoapActionHeader(const char* serviceType, const char* action);

} // namespace sonos
