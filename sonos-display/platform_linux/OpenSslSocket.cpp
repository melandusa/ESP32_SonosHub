#include "OpenSslSocket.h"

#include "platform/Log.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace linux_platform {

namespace {

int connectWithTimeout(const std::string& host, int port, int timeoutMs) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* addresses = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &addresses) != 0) {
        LOGW("TLS: cannot resolve %s", host.c_str());
        return -1;
    }
    int fd = -1;
    for (addrinfo* a = addresses; a && fd < 0; a = a->ai_next) {
        fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0) continue;
        const int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        int rc = ::connect(fd, a->ai_addr, a->ai_addrlen);
        if (rc != 0 && errno == EINPROGRESS) {
            pollfd p{fd, POLLOUT, 0};
            int error = 0;
            socklen_t length = sizeof(error);
            rc = (poll(&p, 1, timeoutMs) == 1 && getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) == 0 &&
                  error == 0)
                     ? 0
                     : -1;
        }
        if (rc != 0) {
            ::close(fd);
            fd = -1;
            continue;
        }
        fcntl(fd, F_SETFL, flags);  // blocking again; reads use poll() for timeouts
    }
    freeaddrinfo(addresses);
    return fd;
}

bool isIpAddress(const std::string& host) {
    in6_addr buffer{};
    return inet_pton(AF_INET, host.c_str(), &buffer) == 1 || inet_pton(AF_INET6, host.c_str(), &buffer) == 1;
}

} // namespace

OpenSslSocket::OpenSslSocket() {
    context_ = SSL_CTX_new(TLS_client_method());
    SSL_CTX_set_verify(context_, SSL_VERIFY_NONE, nullptr);
}

OpenSslSocket::~OpenSslSocket() {
    close();
    SSL_CTX_free(context_);
}

bool OpenSslSocket::connect(const std::string& host, int port, int timeoutMs) {
    close();
    fd_ = connectWithTimeout(host, port, timeoutMs);
    if (fd_ < 0) return false;

    timeval timeout{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));  // bounds the handshake
    setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    ssl_ = SSL_new(context_);
    SSL_set_fd(ssl_, fd_);
    if (!isIpAddress(host)) SSL_set_tlsext_host_name(ssl_, host.c_str());
    if (SSL_connect(ssl_) != 1) {
        LOGW("TLS: handshake with %s:%d failed", host.c_str(), port);
        close();
        return false;
    }
    return true;
}

bool OpenSslSocket::write(const uint8_t* data, std::size_t size) {
    if (!ssl_) return false;
    while (size > 0) {
        const int n = SSL_write(ssl_, data, static_cast<int>(size));
        if (n <= 0) {
            close();
            return false;
        }
        data += n;
        size -= static_cast<std::size_t>(n);
    }
    return true;
}

int OpenSslSocket::read(uint8_t* data, std::size_t size, int timeoutMs) {
    if (!ssl_) return -1;
    if (SSL_pending(ssl_) == 0) {
        pollfd p{fd_, POLLIN, 0};
        const int ready = poll(&p, 1, timeoutMs);
        if (ready == 0) return 0;
        if (ready < 0) {
            close();
            return -1;
        }
    }
    const int n = SSL_read(ssl_, data, static_cast<int>(size));
    if (n > 0) return n;
    const int error = SSL_get_error(ssl_, n);
    if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) return 0;  // e.g. a TLS record was not complete
    close();
    return -1;
}

void OpenSslSocket::close() {
    if (ssl_) {
        SSL_shutdown(ssl_);
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

} // namespace linux_platform
