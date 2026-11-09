#include "hcu/SocketHttp.h"

#include <cctype>
#include <cstdlib>

namespace hcu {

namespace {

constexpr std::size_t kMaxResponseSize = 64 * 1024;

std::string lower(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

// Decodes a "Transfer-Encoding: chunked" body; returns false if it is incomplete.
bool decodeChunked(const std::string& raw, std::string& body) {
    body.clear();
    std::size_t pos = 0;
    while (true) {
        const std::size_t lineEnd = raw.find("\r\n", pos);
        if (lineEnd == std::string::npos) return false;
        const unsigned long size = std::strtoul(raw.c_str() + pos, nullptr, 16);
        pos = lineEnd + 2;
        if (size == 0) return true;
        if (pos + size > raw.size()) return false;
        body.append(raw, pos, size);
        pos += size + 2;
    }
}

} // namespace

platform::HttpResponse postOverSocket(platform::IStreamSocket& socket, const std::string& host, int port,
                                      const std::string& path, const HeaderList& headers,
                                      const std::string& body, int timeoutMs) {
    platform::HttpResponse response;
    if (!socket.connect(host, port, timeoutMs)) return response;

    std::string request = "POST " + path + " HTTP/1.1\r\nHost: " + host + ":" + std::to_string(port) +
                          "\r\nContent-Type: application/json\r\nContent-Length: " +
                          std::to_string(body.size()) + "\r\nConnection: close\r\n";
    for (const auto& [name, value] : headers) request += name + ": " + value + "\r\n";
    request += "\r\n" + body;
    if (!socket.write(reinterpret_cast<const uint8_t*>(request.data()), request.size())) {
        socket.close();
        return response;
    }

    std::string raw;
    uint8_t chunk[1024];
    while (raw.size() < kMaxResponseSize) {
        const int n = socket.read(chunk, sizeof(chunk), timeoutMs);
        if (n <= 0) break;  // closed by the server (expected) or timeout
        raw.append(reinterpret_cast<const char*>(chunk), static_cast<std::size_t>(n));

        // Stop early once a complete response with Content-Length is there.
        const std::size_t headerEnd = raw.find("\r\n\r\n");
        if (headerEnd == std::string::npos) continue;
        const std::string head = lower(raw.substr(0, headerEnd));
        const std::size_t lengthPos = head.find("\r\ncontent-length:");
        if (lengthPos != std::string::npos) {
            const std::size_t length = std::strtoul(head.c_str() + lengthPos + 17, nullptr, 10);
            if (raw.size() >= headerEnd + 4 + length) break;
        }
    }
    socket.close();

    const std::size_t headerEnd = raw.find("\r\n\r\n");
    if (raw.compare(0, 5, "HTTP/") != 0 || headerEnd == std::string::npos) return response;
    const std::size_t space = raw.find(' ');
    response.status = std::atoi(raw.c_str() + space + 1);
    const std::string head = lower(raw.substr(0, headerEnd));
    const std::string rawBody = raw.substr(headerEnd + 4);
    if (head.find("transfer-encoding: chunked") != std::string::npos) {
        if (!decodeChunked(rawBody, response.body)) response.body = rawBody;
    } else {
        response.body = rawBody;
        const std::size_t lengthPos = head.find("\r\ncontent-length:");
        if (lengthPos != std::string::npos) {
            const std::size_t length = std::strtoul(head.c_str() + lengthPos + 17, nullptr, 10);
            if (response.body.size() > length) response.body.resize(length);
        }
    }
    return response;
}

} // namespace hcu
