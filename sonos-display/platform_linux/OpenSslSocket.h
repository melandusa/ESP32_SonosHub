#pragma once

#include "platform/IStreamSocket.h"

#include <openssl/ssl.h>

namespace linux_platform {

// TLS client socket without certificate verification (the HCU's certificate is self-signed).
class OpenSslSocket : public platform::IStreamSocket {
public:
    OpenSslSocket();
    ~OpenSslSocket() override;

    OpenSslSocket(const OpenSslSocket&) = delete;
    OpenSslSocket& operator=(const OpenSslSocket&) = delete;

    bool connect(const std::string& host, int port, int timeoutMs) override;
    bool write(const uint8_t* data, std::size_t size) override;
    int read(uint8_t* data, std::size_t size, int timeoutMs) override;
    void close() override;
    bool isConnected() const override { return ssl_ != nullptr; }

private:
    SSL_CTX* context_ = nullptr;
    SSL* ssl_ = nullptr;
    int fd_ = -1;
};

} // namespace linux_platform
