#pragma once

#include "platform/IStreamSocket.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <string>

// Scripted stream: the test queues incoming chunks; onWrite can answer what the client sends.
class FakeSocket : public platform::IStreamSocket {
public:
    bool connect(const std::string& host, int port, int /*timeoutMs*/) override {
        ++connects;
        lastHost = host;
        lastPort = port;
        connected = acceptConnect;
        return connected;
    }
    bool write(const uint8_t* data, std::size_t size) override {
        if (!connected) return false;
        written.append(reinterpret_cast<const char*>(data), size);
        if (onWrite) onWrite();
        return true;
    }
    int read(uint8_t* data, std::size_t size, int /*timeoutMs*/) override {
        if (!connected) return -1;
        if (incoming.empty()) {
            if (closeWhenEmpty) {
                connected = false;
                return -1;
            }
            return 0;  // timeout
        }
        std::string& chunk = incoming.front();
        const std::size_t n = std::min(size, chunk.size());
        chunk.copy(reinterpret_cast<char*>(data), n);
        chunk.erase(0, n);
        if (chunk.empty()) incoming.pop_front();
        return static_cast<int>(n);
    }
    void close() override { connected = false; }
    bool isConnected() const override { return connected; }

    void push(const std::string& chunk) { incoming.push_back(chunk); }

    bool acceptConnect = true;
    bool connected = false;
    bool closeWhenEmpty = false;
    int connects = 0;
    std::string lastHost;
    int lastPort = 0;
    std::string written;
    std::deque<std::string> incoming;
    std::function<void()> onWrite;
};

// Server -> client frame (unmasked).
inline std::string serverFrame(const std::string& payload, uint8_t opcode = 0x1, bool fin = true) {
    std::string frame;
    frame += static_cast<char>((fin ? 0x80 : 0x00) | opcode);
    const std::size_t size = payload.size();
    if (size < 126) {
        frame += static_cast<char>(size);
    } else if (size <= 0xFFFF) {
        frame += static_cast<char>(126);
        frame += static_cast<char>((size >> 8) & 0xFF);
        frame += static_cast<char>(size & 0xFF);
    } else {
        frame += static_cast<char>(127);
        for (int shift = 56; shift >= 0; shift -= 8) {
            frame += static_cast<char>((static_cast<uint64_t>(size) >> shift) & 0xFF);
        }
    }
    return frame + payload;
}

struct ClientFrame {
    uint8_t opcode = 0;
    std::string payload;
};

// Decodes the client's (masked) frames from `data`, consuming them; stops at an incomplete frame.
inline std::deque<ClientFrame> takeClientFrames(std::string& data) {
    std::deque<ClientFrame> frames;
    while (data.size() >= 2) {
        const auto* p = reinterpret_cast<const uint8_t*>(data.data());
        uint64_t length = p[1] & 0x7F;
        std::size_t header = 2;
        if (length == 126) {
            if (data.size() < 4) break;
            length = (static_cast<uint64_t>(p[2]) << 8) | p[3];
            header = 4;
        } else if (length == 127) {
            if (data.size() < 10) break;
            length = 0;
            for (int i = 0; i < 8; ++i) length = (length << 8) | p[2 + i];
            header = 10;
        }
        const bool masked = (p[1] & 0x80) != 0;
        const std::size_t maskPos = header;
        if (masked) header += 4;
        if (data.size() < header + length) break;
        ClientFrame frame;
        frame.opcode = p[0] & 0x0F;
        frame.payload = data.substr(header, static_cast<std::size_t>(length));
        if (masked) {
            for (std::size_t i = 0; i < frame.payload.size(); ++i) {
                frame.payload[i] = static_cast<char>(frame.payload[i] ^ p[maskPos + i % 4]);
            }
        }
        frames.push_back(frame);
        data.erase(0, header + static_cast<std::size_t>(length));
    }
    return frames;
}

inline const char* kSwitchingProtocols =
    "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
    "Sec-WebSocket-Accept: x\r\n\r\n";
