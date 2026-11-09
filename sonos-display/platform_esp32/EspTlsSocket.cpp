#include "EspTlsSocket.h"

#include "platform/Log.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include <algorithm>
#include <cstring>

namespace esp_platform {

namespace {

constexpr uint32_t kMdnsTimeoutMs = 3000;
bool mdnsStarted = false;

bool endsWith(const std::string& text, const char* suffix) {
    const std::size_t length = std::strlen(suffix);
    return text.size() >= length && text.compare(text.size() - length, length, suffix) == 0;
}

bool resolve(const std::string& host, IPAddress& ip) {
    if (ip.fromString(host.c_str())) return true;
    if (endsWith(host, ".local")) {
        if (!mdnsStarted) beginMdns();
        ip = MDNS.queryHost(host.substr(0, host.size() - 6).c_str(), kMdnsTimeoutMs);
        if (ip != IPAddress(0, 0, 0, 0)) return true;
        LOGW("TLS: %s not found via mDNS", host.c_str());
        return false;
    }
    if (WiFi.hostByName(host.c_str(), ip) == 1) return true;
    LOGW("TLS: cannot resolve %s", host.c_str());
    return false;
}

} // namespace

void beginMdns() {
    if (mdnsStarted) return;
    mdnsStarted = MDNS.begin("sonos-display");
    if (!mdnsStarted) LOGW("mDNS start failed");
}

bool EspTlsSocket::connect(const std::string& host, int port, int timeoutMs) {
    close();
    IPAddress ip;
    if (!resolve(host, ip)) return false;
    client_.setInsecure();  // the HCU's certificate is self-signed
    client_.setHandshakeTimeout(static_cast<unsigned long>(std::max(1, timeoutMs / 1000)));
    if (!client_.connect(ip, static_cast<uint16_t>(port), timeoutMs)) {
        LOGW("TLS: connection to %s:%d failed", host.c_str(), port);
        return false;
    }
    return true;
}

bool EspTlsSocket::write(const uint8_t* data, std::size_t size) {
    while (size > 0) {
        const size_t n = client_.write(data, size);
        if (n == 0) {
            close();
            return false;
        }
        data += n;
        size -= n;
    }
    return true;
}

int EspTlsSocket::read(uint8_t* data, std::size_t size, int timeoutMs) {
    const uint32_t start = millis();
    while (true) {
        const int available = client_.available();
        if (available > 0) {
            const int n = client_.read(data, std::min(size, static_cast<std::size_t>(available)));
            return n > 0 ? n : -1;
        }
        if (!client_.connected()) return -1;
        if (millis() - start >= static_cast<uint32_t>(timeoutMs)) return 0;
        delay(5);
    }
}

void EspTlsSocket::close() { client_.stop(); }

bool EspTlsSocket::isConnected() const { return const_cast<WiFiClientSecure&>(client_).connected(); }

void* SpiRamAllocator::allocate(size_t size) { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }

void SpiRamAllocator::deallocate(void* pointer) { heap_caps_free(pointer); }

void* SpiRamAllocator::reallocate(void* pointer, size_t newSize) {
    return heap_caps_realloc(pointer, newSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

} // namespace esp_platform
