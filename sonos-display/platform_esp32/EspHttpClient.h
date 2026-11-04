#pragma once

#include "platform/IHttpClient.h"

#include <HTTPClient.h>
#include <WiFiClient.h>

namespace esp_platform {

// Arduino HTTPClient with keep-alive. Use from the worker task only.
class EspHttpClient : public platform::IHttpClient {
public:
    EspHttpClient();

    platform::HttpResponse post(const std::string& url, const platform::HttpHeaders& headers,
                                const std::string& body, int timeoutMs) override;
    platform::HttpResponse get(const std::string& url, int timeoutMs) override;

private:
    bool begin(const std::string& url, int timeoutMs);
    platform::HttpResponse finish(int code);

    WiFiClient client_;
    HTTPClient http_;
};

} // namespace esp_platform
