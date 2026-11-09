#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace platform {

// A TLS byte stream to a host in the local network (Linux: OpenSSL, ESP32: WiFiClientSecure).
// The Homematic IP HCU uses a self-signed certificate, so the certificate is not verified.
// Used by one thread at a time.
class IStreamSocket {
public:
    virtual ~IStreamSocket() = default;
    // Closes a previous connection first. Host may be an IP address or a host name.
    virtual bool connect(const std::string& host, int port, int timeoutMs) = 0;
    // Writes all bytes; false on error (the connection is closed then).
    virtual bool write(const uint8_t* data, std::size_t size) = 0;
    // Reads up to size bytes. Returns the number of bytes read, 0 if nothing arrived within
    // timeoutMs, -1 if the connection is closed or failed.
    virtual int read(uint8_t* data, std::size_t size, int timeoutMs) = 0;
    virtual void close() = 0;
    virtual bool isConnected() const = 0;
};

} // namespace platform
