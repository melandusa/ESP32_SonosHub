#pragma once

#include "platform/IDiscovery.h"
#include "platform/IHttpClient.h"
#include "sonos/Types.h"

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace linux_platform {

// Simulated Sonos speaker for --mock. Answers the same SOAP requests as a real
// ZonePlayer (including double-escaped favorites), so the full client stack is exercised.
class MockSonos : public platform::IHttpClient, public platform::IDiscovery {
public:
    explicit MockSonos(int latencyMs = 150);

    platform::HttpResponse post(const std::string& url, const platform::HttpHeaders& headers,
                                const std::string& body, int timeoutMs) override;
    platform::HttpResponse get(const std::string& url, int timeoutMs) override;
    std::vector<std::string> discover(int timeoutMs) override;

    // Simulates an unplugged speaker: requests time out, discovery finds nothing.
    void setOnline(bool online) { online_ = online; }
    bool isOnline() const { return online_; }

private:
    struct Station {
        std::string title;
        std::string uri;
    };

    std::string handle(const std::string& action, const std::string& requestBody);
    std::string browseResponse() const;
    std::string itemMetadata(const Station& station) const;
    void simulateLatency() const;

    int latencyMs_;
    std::atomic<bool> online_{true};
    std::mutex mutex_;
    std::vector<Station> stations_;
    sonos::TransportState transport_ = sonos::TransportState::Stopped;
    std::string currentUri_;
    std::string currentMetadata_;
    int volume_ = 25;
};

// Exposes the discovery side of a MockSonos owned elsewhere.
class MockDiscoveryAdapter : public platform::IDiscovery {
public:
    explicit MockDiscoveryAdapter(MockSonos& mock) : mock_(mock) {}
    std::vector<std::string> discover(int timeoutMs) override { return mock_.discover(timeoutMs); }

private:
    MockSonos& mock_;
};

} // namespace linux_platform
