#include "PosixSsdpDiscovery.h"

#include "platform/Log.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstring>

namespace linux_platform {

namespace {

constexpr const char* kSearchRequest =
    "M-SEARCH * HTTP/1.1\r\n"
    "HOST: 239.255.255.250:1900\r\n"
    "MAN: \"ssdp:discover\"\r\n"
    "MX: 1\r\n"
    "ST: urn:schemas-upnp-org:device:ZonePlayer:1\r\n"
    "\r\n";

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// Value of a header line (case-insensitive name), trimmed; "" if missing.
std::string headerValue(const std::string& response, const std::string& name) {
    const std::string lower = toLower(response);
    const std::string key = "\n" + toLower(name) + ":";
    const auto pos = lower.find(key);
    if (pos == std::string::npos) return {};
    auto start = pos + key.size();
    auto end = response.find_first_of("\r\n", start);
    if (end == std::string::npos) end = response.size();
    while (start < end && response[start] == ' ') ++start;
    return response.substr(start, end - start);
}

} // namespace

std::string parseSsdpResponse(const std::string& response) {
    if (headerValue(response, "ST").find("ZonePlayer") == std::string::npos) return {};
    const std::string location = headerValue(response, "LOCATION");  // http://<ip>:1400/xml/...
    const std::string prefix = "http://";
    if (location.compare(0, prefix.size(), prefix) != 0) return {};
    const auto hostEnd = location.find_first_of(":/", prefix.size());
    return location.substr(prefix.size(), hostEnd - prefix.size());
}

std::vector<std::string> PosixSsdpDiscovery::discover(int timeoutMs) {
    std::vector<std::string> found;

    const int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LOGE("SSDP: socket() failed: %s", std::strerror(errno));
        return found;
    }
    const unsigned char ttl = 2;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(1900);
    inet_pton(AF_INET, "239.255.255.250", &target.sin_addr);

    // UDP may get lost: send the request twice.
    for (int i = 0; i < 2; ++i) {
        if (sendto(sock, kSearchRequest, std::strlen(kSearchRequest), 0,
                   reinterpret_cast<const sockaddr*>(&target), sizeof(target)) < 0) {
            LOGE("SSDP: sendto() failed: %s", std::strerror(errno));
        }
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    char buffer[1536];
    while (true) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   deadline - std::chrono::steady_clock::now())
                                   .count();
        if (remaining <= 0) break;
        pollfd pfd{sock, POLLIN, 0};
        if (poll(&pfd, 1, static_cast<int>(remaining)) <= 0) break;

        const ssize_t received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) continue;
        const std::string ip = parseSsdpResponse(std::string(buffer, static_cast<size_t>(received)));
        if (!ip.empty() && std::find(found.begin(), found.end(), ip) == found.end()) {
            LOGD("SSDP: response from %s", ip.c_str());
            found.push_back(ip);
        }
    }

    close(sock);
    return found;
}

} // namespace linux_platform
