#pragma once

#include "platform/IStreamSocket.h"

#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

namespace esp_platform {

// TLS socket for the Homematic IP HCU: no certificate check (self-signed), host names
// ending in ".local" are resolved via mDNS.
class EspTlsSocket : public platform::IStreamSocket {
public:
    bool connect(const std::string& host, int port, int timeoutMs) override;
    bool write(const uint8_t* data, std::size_t size) override;
    int read(uint8_t* data, std::size_t size, int timeoutMs) override;
    void close() override;
    bool isConnected() const override;

private:
    WiFiClientSecure client_;
};

// Starts mDNS (needed to resolve hcu1-XXXX.local). Call once after the WLAN is connected.
void beginMdns();

// Puts parsed JSON (the HCU system state) into PSRAM instead of the scarce internal RAM.
class SpiRamAllocator : public ArduinoJson::Allocator {
public:
    void* allocate(size_t size) override;
    void deallocate(void* pointer) override;
    void* reallocate(void* pointer, size_t newSize) override;
};

} // namespace esp_platform
