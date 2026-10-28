#pragma once

#include "platform/IHttpClient.h"

typedef void CURL;

namespace linux_platform {

// libcurl based client. Keeps one handle so connections to the speaker are reused.
// Not thread-safe: use from the worker thread only.
class CurlHttpClient : public platform::IHttpClient {
public:
    CurlHttpClient();
    ~CurlHttpClient() override;

    CurlHttpClient(const CurlHttpClient&) = delete;
    CurlHttpClient& operator=(const CurlHttpClient&) = delete;

    platform::HttpResponse post(const std::string& url, const platform::HttpHeaders& headers,
                                const std::string& body, int timeoutMs) override;
    platform::HttpResponse get(const std::string& url, int timeoutMs) override;

private:
    platform::HttpResponse perform(int timeoutMs);

    CURL* curl_;
};

} // namespace linux_platform
