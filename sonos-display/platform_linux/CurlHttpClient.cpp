#include "CurlHttpClient.h"

#include "platform/Log.h"

#include <curl/curl.h>

namespace linux_platform {

namespace {

size_t appendToString(char* data, size_t size, size_t count, void* userData) {
    static_cast<std::string*>(userData)->append(data, size * count);
    return size * count;
}

} // namespace

CurlHttpClient::CurlHttpClient() : curl_(curl_easy_init()) {}

CurlHttpClient::~CurlHttpClient() { curl_easy_cleanup(curl_); }

platform::HttpResponse CurlHttpClient::post(const std::string& url,
                                            const platform::HttpHeaders& headers,
                                            const std::string& body, int timeoutMs) {
    curl_easy_reset(curl_);
    curl_slist* headerList = nullptr;
    for (const auto& [name, value] : headers) {
        headerList = curl_slist_append(headerList, (name + ": " + value).c_str());
    }
    // Sonos does not need "Expect: 100-continue"; skipping it saves a round trip.
    headerList = curl_slist_append(headerList, "Expect:");

    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headerList);
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    platform::HttpResponse response = perform(timeoutMs);
    curl_slist_free_all(headerList);
    return response;
}

platform::HttpResponse CurlHttpClient::get(const std::string& url, int timeoutMs) {
    curl_easy_reset(curl_);
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    return perform(timeoutMs);
}

platform::HttpResponse CurlHttpClient::perform(int timeoutMs) {
    platform::HttpResponse response;
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS, static_cast<long>(timeoutMs));
    curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(timeoutMs));
    curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, appendToString);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response.body);

    const CURLcode code = curl_easy_perform(curl_);
    if (code != CURLE_OK) {
        LOGD("HTTP: %s", curl_easy_strerror(code));
        response.status = 0;
        response.body.clear();
        return response;
    }
    long status = 0;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &status);
    response.status = static_cast<int>(status);
    return response;
}

} // namespace linux_platform
