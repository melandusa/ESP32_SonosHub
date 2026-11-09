#include "FakeSocket.h"
#include "hcu/SocketHttp.h"
#include "hcu/WebSocketClient.h"

#include <doctest/doctest.h>

using hcu::WebSocketClient;

namespace {

uint32_t fixedRandom() { return 0x12345678u; }

struct Fixture {
    FakeSocket socket;
    WebSocketClient ws{socket, fixedRandom};

    bool open() {
        socket.push(kSwitchingProtocols);
        return ws.open("hcu1-1234.local", 9001, "/", {{"authtoken", "ABC"}, {"plugin-id", "p"}}, 1000);
    }
};

} // namespace

TEST_CASE("base64") {
    const auto* text = reinterpret_cast<const uint8_t*>("foobar");
    CHECK(hcu::base64Encode(text, 0) == "");
    CHECK(hcu::base64Encode(text, 1) == "Zg==");
    CHECK(hcu::base64Encode(text, 2) == "Zm8=");
    CHECK(hcu::base64Encode(text, 3) == "Zm9v");
    CHECK(hcu::base64Encode(text, 6) == "Zm9vYmFy");
}

TEST_CASE("WebSocket handshake sends upgrade request with headers") {
    Fixture f;
    REQUIRE(f.open());
    CHECK(f.ws.isOpen());
    CHECK(f.socket.lastPort == 9001);
    CHECK(f.socket.written.rfind("GET / HTTP/1.1\r\n", 0) == 0);
    CHECK(f.socket.written.find("Host: hcu1-1234.local:9001\r\n") != std::string::npos);
    CHECK(f.socket.written.find("Upgrade: websocket\r\n") != std::string::npos);
    CHECK(f.socket.written.find("Sec-WebSocket-Version: 13\r\n") != std::string::npos);
    CHECK(f.socket.written.find("authtoken: ABC\r\n") != std::string::npos);
    CHECK(f.socket.written.find("plugin-id: p\r\n") != std::string::npos);
}

TEST_CASE("WebSocket handshake rejected") {
    Fixture f;
    f.socket.push("HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n\r\n");
    CHECK_FALSE(f.ws.open("h", 9001, "/", {}, 1000));
    CHECK(f.ws.httpStatus() == 401);
    CHECK_FALSE(f.ws.isOpen());
}

TEST_CASE("WebSocket handshake without answer") {
    Fixture f;
    CHECK_FALSE(f.ws.open("h", 9001, "/", {}, 1000));
    CHECK(f.ws.httpStatus() == 0);
}

TEST_CASE("WebSocket reads frames of all length encodings, also in pieces") {
    Fixture f;
    // First frame arrives together with the handshake answer
    f.socket.push(std::string(kSwitchingProtocols) + serverFrame("hello"));
    REQUIRE(f.ws.open("h", 9001, "/", {}, 1000));

    std::string message;
    CHECK(f.ws.readMessage(message, 10) == WebSocketClient::ReadStatus::Message);
    CHECK(message == "hello");

    const std::string medium(300, 'm');
    const std::string large(70000, 'L');
    const std::string frames = serverFrame(medium) + serverFrame(large);
    for (std::size_t i = 0; i < frames.size(); i += 1000) f.socket.push(frames.substr(i, 1000));
    CHECK(f.ws.readMessage(message, 10) == WebSocketClient::ReadStatus::Message);
    CHECK(message == medium);
    CHECK(f.ws.readMessage(message, 10) == WebSocketClient::ReadStatus::Message);
    CHECK(message == large);
    CHECK(f.ws.readMessage(message, 10) == WebSocketClient::ReadStatus::Timeout);
    CHECK(f.ws.isOpen());
}

TEST_CASE("WebSocket joins fragments and answers pings in between") {
    Fixture f;
    REQUIRE(f.open());
    f.socket.written.clear();
    f.socket.push(serverFrame("{\"a\":", 0x1, false) + serverFrame("ping!", 0x9) + serverFrame("1}", 0x0, true));
    std::string message;
    CHECK(f.ws.readMessage(message, 10) == WebSocketClient::ReadStatus::Message);
    CHECK(message == "{\"a\":1}");
    auto sent = takeClientFrames(f.socket.written);
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].opcode == 0xA);
    CHECK(sent[0].payload == "ping!");
}

TEST_CASE("WebSocket close frame and broken connection") {
    Fixture f;
    REQUIRE(f.open());
    f.socket.push(serverFrame("", 0x8));
    std::string message;
    CHECK(f.ws.readMessage(message, 10) == WebSocketClient::ReadStatus::Closed);
    CHECK_FALSE(f.ws.isOpen());

    REQUIRE(f.open());
    f.socket.push(serverFrame("truncated").substr(0, 5));
    f.socket.closeWhenEmpty = true;
    CHECK(f.ws.readMessage(message, 10) == WebSocketClient::ReadStatus::Closed);
    CHECK_FALSE(f.ws.isOpen());
}

TEST_CASE("WebSocket sends masked text frames") {
    Fixture f;
    REQUIRE(f.open());
    f.socket.written.clear();
    const std::string longText(200, 'x');
    CHECK(f.ws.sendText("{\"type\":\"X\"}"));
    CHECK(f.ws.sendText(longText));
    auto sent = takeClientFrames(f.socket.written);
    REQUIRE(sent.size() == 2);
    CHECK(sent[0].opcode == 0x1);
    CHECK(sent[0].payload == "{\"type\":\"X\"}");
    CHECK(sent[1].payload == longText);
    CHECK(f.socket.written.empty());
}

TEST_CASE("POST over socket parses Content-Length and chunked answers") {
    FakeSocket socket;
    socket.closeWhenEmpty = true;
    socket.push("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 13\r\n\r\n");
    socket.push("{\"a\":\"token\"}");
    platform::HttpResponse response =
        hcu::postOverSocket(socket, "10.0.0.5", 6969, "/x", {{"VERSION", "12"}}, "{\"k\":1}", 1000);
    CHECK(response.status == 200);
    CHECK(response.body == "{\"a\":\"token\"}");
    CHECK(socket.written.rfind("POST /x HTTP/1.1\r\n", 0) == 0);
    CHECK(socket.written.find("VERSION: 12\r\n") != std::string::npos);
    CHECK(socket.written.find("Content-Length: 7\r\n") != std::string::npos);
    CHECK(socket.written.substr(socket.written.size() - 7) == "{\"k\":1}");

    FakeSocket chunked;
    chunked.closeWhenEmpty = true;
    chunked.push("HTTP/1.1 403 Forbidden\r\nTransfer-Encoding: chunked\r\n\r\n4\r\n{\"e\"\r\n3\r\n:1}\r\n0\r\n\r\n");
    response = hcu::postOverSocket(chunked, "h", 6969, "/x", {}, "", 1000);
    CHECK(response.status == 403);
    CHECK(response.body == "{\"e\":1}");

    FakeSocket refused;
    refused.acceptConnect = false;
    CHECK(hcu::postOverSocket(refused, "h", 6969, "/x", {}, "", 1000).status == 0);
}
