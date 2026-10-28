#include "sonos/SoapBuilder.h"

namespace sonos {

std::string escapeXml(const std::string& text) {
    std::string out;
    out.reserve(text.size() + text.size() / 8);
    for (char c : text) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        default: out += c; break;
        }
    }
    return out;
}

std::string buildSoapEnvelope(const char* serviceType, const char* action, const SoapArgs& args) {
    std::string body;
    body.reserve(320);
    body += "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
            "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
            " s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
            "<s:Body><u:";
    body += action;
    body += " xmlns:u=\"";
    body += serviceType;
    body += "\">";
    for (const auto& [name, value] : args) {
        body += '<';
        body += name;
        body += '>';
        body += escapeXml(value);
        body += "</";
        body += name;
        body += '>';
    }
    body += "</u:";
    body += action;
    body += "></s:Body></s:Envelope>";
    return body;
}

std::string buildSoapActionHeader(const char* serviceType, const char* action) {
    std::string header = "\"";
    header += serviceType;
    header += '#';
    header += action;
    header += '"';
    return header;
}

} // namespace sonos
