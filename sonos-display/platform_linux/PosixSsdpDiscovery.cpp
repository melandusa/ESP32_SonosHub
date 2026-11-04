#include "PosixSsdpDiscovery.h"

#include "platform/Log.h"
#include "sonos/Ssdp.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>

namespace linux_platform {

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
    target.sin_port = htons(sonos::kSsdpPort);
    inet_pton(AF_INET, sonos::kSsdpMulticastAddress, &target.sin_addr);

    // UDP may get lost: send the request twice.
    for (int i = 0; i < 2; ++i) {
        if (sendto(sock, sonos::kSsdpSearchRequest, std::strlen(sonos::kSsdpSearchRequest), 0,
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
        const std::string ip = sonos::parseSsdpResponse(std::string(buffer, static_cast<size_t>(received)));
        if (!ip.empty() && std::find(found.begin(), found.end(), ip) == found.end()) {
            LOGD("SSDP: response from %s", ip.c_str());
            found.push_back(ip);
        }
    }

    close(sock);
    return found;
}

} // namespace linux_platform
