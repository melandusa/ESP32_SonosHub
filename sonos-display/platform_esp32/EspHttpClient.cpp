#include "EspHttpClient.h"

#include "platform/Log.h"

namespace esp_platform {

EspHttpClient::EspHttpClient() { http_.setReuse(true); }

bool EspHttpClient::begin(const std::string& url, int timeoutMs) {
    http_.setConnectTimeout(timeoutMs);
    http_.setTimeout(static_cast<uint16_t>(timeoutMs));
    return http_.begin(client_, url.c_str());
}

platform::HttpResponse EspHttpClient::finish(int code) {
    platform::HttpResponse response;
    if (code <= 0) {
        LOGD("HTTP: %s", HTTPClient::errorToString(code).c_str());
    } else {
        response.status = code;
        const String payload = http_.getString();
        response.body.assign(payload.c_str(), payload.length());
    }
    http_.end();
    return response;
}

platform::HttpResponse EspHttpClient::post(const std::string& url,
                                           const platform::HttpHeaders& headers,
                                           const std::string& body, int timeoutMs) {
    if (!begin(url, timeoutMs)) return {};
    for (const auto& [name, value] : headers) http_.addHeader(name.c_str(), value.c_str());
    const int code = http_.POST(reinterpret_cast<uint8_t*>(const_cast<char*>(body.data())), body.size());
    return finish(code);
}

platform::HttpResponse EspHttpClient::get(const std::string& url, int timeoutMs) {
    if (!begin(url, timeoutMs)) return {};
    return finish(http_.GET());
}

} // namespace esp_platform
