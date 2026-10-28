#pragma once

#include <string>
#include <utility>
#include <vector>

namespace platform {

struct HttpResponse {
    int status = 0;  // HTTP status code, 0 on transport error (timeout, refused, ...)
    std::string body;
};

using HttpHeaders = std::vector<std::pair<std::string, std::string>>;

class IHttpClient {
public:
    virtual ~IHttpClient() = default;
    virtual HttpResponse post(const std::string& url, const HttpHeaders& headers,
                              const std::string& body, int timeoutMs) = 0;
    virtual HttpResponse get(const std::string& url, int timeoutMs) = 0;
};

} // namespace platform
