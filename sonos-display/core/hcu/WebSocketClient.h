#pragma once

#include "platform/IStreamSocket.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace hcu {

using HeaderList = std::vector<std::pair<std::string, std::string>>;

// Minimal WebSocket client (RFC 6455) on top of a platform stream: text messages,
// fragmentation, ping/pong and close. No extensions.
class WebSocketClient {
public:
    enum class ReadStatus { Message, Timeout, Closed };

    // random: source for the handshake key and the frame masks.
    WebSocketClient(platform::IStreamSocket& socket, std::function<uint32_t()> random);

    // Connects and performs the upgrade handshake. On failure, httpStatus() holds the HTTP
    // status of the server's answer (0 if there was none).
    bool open(const std::string& host, int port, const std::string& path, const HeaderList& headers,
              int timeoutMs);
    bool isOpen() const { return open_; }
    int httpStatus() const { return httpStatus_; }

    bool sendText(const std::string& text);
    // Waits up to timeoutMs for the start of the next text (or binary) message; once a
    // message has started, the rest may take up to kFrameTimeoutMs per read.
    ReadStatus readMessage(std::string& message, int timeoutMs);
    void close();

    static constexpr std::size_t kMaxMessageSize = 4 * 1024 * 1024;
    static constexpr int kFrameTimeoutMs = 10000;

private:
    enum Opcode : uint8_t { Continuation = 0x0, Text = 0x1, Binary = 0x2, Close = 0x8, Ping = 0x9, Pong = 0xA };

    bool sendFrame(uint8_t opcode, const uint8_t* payload, std::size_t size);
    // Makes at least `count` unread bytes available. False on timeout or closed connection.
    bool fill(std::size_t count, int timeoutMs, bool& timedOut);
    void consume(std::size_t count);
    void fail();

    platform::IStreamSocket& socket_;
    std::function<uint32_t()> random_;
    std::string buffer_;      // received, not yet consumed bytes start at readPos_
    std::size_t readPos_ = 0;
    bool open_ = false;
    int httpStatus_ = 0;
};

std::string base64Encode(const uint8_t* data, std::size_t size);

} // namespace hcu
