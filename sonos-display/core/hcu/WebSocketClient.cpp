#include "hcu/WebSocketClient.h"

#include "platform/Log.h"

#include <cstdlib>
#include <cstring>

namespace hcu {

namespace {
constexpr std::size_t kMaxHandshakeSize = 16 * 1024;
constexpr std::size_t kReadChunk = 4096;
} // namespace

std::string base64Encode(const uint8_t* data, std::size_t size) {
    static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((size + 2) / 3 * 4);
    for (std::size_t i = 0; i < size; i += 3) {
        const uint32_t b0 = data[i];
        const uint32_t b1 = i + 1 < size ? data[i + 1] : 0;
        const uint32_t b2 = i + 2 < size ? data[i + 2] : 0;
        const uint32_t triple = (b0 << 16) | (b1 << 8) | b2;
        out += kAlphabet[(triple >> 18) & 0x3F];
        out += kAlphabet[(triple >> 12) & 0x3F];
        out += i + 1 < size ? kAlphabet[(triple >> 6) & 0x3F] : '=';
        out += i + 2 < size ? kAlphabet[triple & 0x3F] : '=';
    }
    return out;
}

WebSocketClient::WebSocketClient(platform::IStreamSocket& socket, std::function<uint32_t()> random)
    : socket_(socket), random_(std::move(random)) {}

bool WebSocketClient::open(const std::string& host, int port, const std::string& path,
                           const HeaderList& headers, int timeoutMs) {
    close();
    httpStatus_ = 0;
    buffer_.clear();
    readPos_ = 0;
    if (!socket_.connect(host, port, timeoutMs)) return false;

    uint8_t key[16];
    for (std::size_t i = 0; i < sizeof(key); i += 4) {
        const uint32_t r = random_();
        std::memcpy(key + i, &r, 4);
    }
    std::string request = "GET " + path + " HTTP/1.1\r\nHost: " + host + ":" + std::to_string(port) +
                          "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " +
                          base64Encode(key, sizeof(key)) + "\r\nSec-WebSocket-Version: 13\r\n";
    for (const auto& [name, value] : headers) request += name + ": " + value + "\r\n";
    request += "\r\n";
    if (!socket_.write(reinterpret_cast<const uint8_t*>(request.data()), request.size())) return false;

    // Response header; bytes after it already belong to the first frames.
    std::size_t headerEnd = std::string::npos;
    while ((headerEnd = buffer_.find("\r\n\r\n")) == std::string::npos) {
        bool timedOut = false;
        if (buffer_.size() > kMaxHandshakeSize || !fill(buffer_.size() + 1, timeoutMs, timedOut)) {
            LOGW("WebSocket: no handshake answer from %s:%d", host.c_str(), port);
            socket_.close();
            return false;
        }
    }
    const std::string statusLine = buffer_.substr(0, buffer_.find("\r\n"));
    const std::size_t space = statusLine.find(' ');
    if (space != std::string::npos) httpStatus_ = std::atoi(statusLine.c_str() + space + 1);
    consume(headerEnd + 4);
    if (httpStatus_ != 101) {
        LOGW("WebSocket: handshake with %s:%d rejected (%s)", host.c_str(), port, statusLine.c_str());
        socket_.close();
        return false;
    }
    open_ = true;
    return true;
}

void WebSocketClient::close() {
    if (open_) sendFrame(Close, nullptr, 0);
    open_ = false;
    socket_.close();
}

void WebSocketClient::fail() {
    open_ = false;
    socket_.close();
}

bool WebSocketClient::fill(std::size_t count, int timeoutMs, bool& timedOut) {
    timedOut = false;
    uint8_t chunk[kReadChunk];
    while (buffer_.size() - readPos_ < count) {
        const int n = socket_.read(chunk, sizeof(chunk), timeoutMs);
        if (n < 0) return false;
        if (n == 0) {
            timedOut = true;
            return false;
        }
        buffer_.append(reinterpret_cast<const char*>(chunk), static_cast<std::size_t>(n));
    }
    return true;
}

void WebSocketClient::consume(std::size_t count) {
    readPos_ += count;
    if (readPos_ == buffer_.size()) {
        buffer_.clear();
        readPos_ = 0;
    } else if (readPos_ > kReadChunk) {
        buffer_.erase(0, readPos_);
        readPos_ = 0;
    }
}

bool WebSocketClient::sendFrame(uint8_t opcode, const uint8_t* payload, std::size_t size) {
    std::string frame;
    frame.reserve(size + 14);
    frame += static_cast<char>(0x80 | opcode);  // FIN, no fragmentation
    if (size < 126) {
        frame += static_cast<char>(0x80 | size);
    } else if (size <= 0xFFFF) {
        frame += static_cast<char>(0x80 | 126);
        frame += static_cast<char>((size >> 8) & 0xFF);
        frame += static_cast<char>(size & 0xFF);
    } else {
        frame += static_cast<char>(0x80 | 127);
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame += static_cast<char>((static_cast<uint64_t>(size) >> shift) & 0xFF);
        }
    }
    uint8_t mask[4];
    const uint32_t r = random_();
    std::memcpy(mask, &r, 4);
    frame.append(reinterpret_cast<const char*>(mask), 4);
    for (std::size_t i = 0; i < size; ++i) frame += static_cast<char>(payload[i] ^ mask[i % 4]);

    if (!socket_.write(reinterpret_cast<const uint8_t*>(frame.data()), frame.size())) {
        fail();
        return false;
    }
    return true;
}

bool WebSocketClient::sendText(const std::string& text) {
    if (!open_) return false;
    return sendFrame(Text, reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

WebSocketClient::ReadStatus WebSocketClient::readMessage(std::string& message, int timeoutMs) {
    message.clear();
    if (!open_) return ReadStatus::Closed;
    bool inMessage = false;

    while (true) {
        bool timedOut = false;
        if (!fill(2, inMessage ? kFrameTimeoutMs : timeoutMs, timedOut)) {
            if (timedOut && !inMessage && buffer_.size() == readPos_) return ReadStatus::Timeout;
            if (timedOut && !inMessage) continue;  // a frame header has started: keep waiting
            fail();
            return ReadStatus::Closed;
        }
        inMessage = true;
        const auto* head = reinterpret_cast<const uint8_t*>(buffer_.data() + readPos_);
        const bool fin = (head[0] & 0x80) != 0;
        const uint8_t opcode = head[0] & 0x0F;
        const bool masked = (head[1] & 0x80) != 0;
        uint64_t length = head[1] & 0x7F;
        std::size_t headerSize = 2;
        if (length == 126) {
            headerSize = 4;
        } else if (length == 127) {
            headerSize = 10;
        }
        if (masked) headerSize += 4;
        if (!fill(headerSize, kFrameTimeoutMs, timedOut)) {
            fail();
            return ReadStatus::Closed;
        }
        head = reinterpret_cast<const uint8_t*>(buffer_.data() + readPos_);
        if (length == 126) {
            length = (static_cast<uint64_t>(head[2]) << 8) | head[3];
        } else if (length == 127) {
            length = 0;
            for (int i = 0; i < 8; ++i) length = (length << 8) | head[2 + i];
        }
        if (length > kMaxMessageSize || message.size() + length > kMaxMessageSize) {
            LOGW("WebSocket: message too large (%llu bytes)", static_cast<unsigned long long>(length));
            fail();
            return ReadStatus::Closed;
        }
        uint8_t mask[4] = {0, 0, 0, 0};
        if (masked) std::memcpy(mask, head + headerSize - 4, 4);

        const auto size = static_cast<std::size_t>(length);
        if (!fill(headerSize + size, kFrameTimeoutMs, timedOut)) {
            fail();
            return ReadStatus::Closed;
        }
        std::string payload = buffer_.substr(readPos_ + headerSize, size);
        if (masked) {
            for (std::size_t i = 0; i < size; ++i) payload[i] = static_cast<char>(payload[i] ^ mask[i % 4]);
        }
        consume(headerSize + size);

        switch (opcode) {
        case Ping:
            sendFrame(Pong, reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
            break;
        case Pong: break;
        case Close:
            sendFrame(Close, nullptr, 0);
            fail();
            return ReadStatus::Closed;
        case Text:
        case Binary:
        case Continuation:
            message += payload;
            if (fin) return ReadStatus::Message;
            break;
        default:
            LOGW("WebSocket: unknown opcode %u", static_cast<unsigned>(opcode));
            fail();
            return ReadStatus::Closed;
        }
        // Control frames between fragments do not end the message.
        if (message.empty() && (opcode == Ping || opcode == Pong)) inMessage = false;
    }
}

} // namespace hcu
