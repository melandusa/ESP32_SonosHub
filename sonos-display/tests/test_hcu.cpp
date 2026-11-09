#include "FakeSocket.h"
#include "TestUtil.h"
#include "hcu/HcuClient.h"

#include <ArduinoJson.h>
#include <doctest/doctest.h>

#include <string>
#include <vector>

using home::Device;
using home::DeviceKind;
using home::HomeStatus;
using home::ShutterControl;

namespace {

class FakeClock : public platform::IClock {
public:
    uint64_t nowMs() const override { return now += 100; }  // advances on every look
    mutable uint64_t now = 0;
};

// Plays the HCU: answers the handshake, getSystemState and control requests.
class FakeHcu {
public:
    explicit FakeHcu(FakeSocket& socket) : socket_(socket) {
        socket_.onWrite = [this] { onWrite(); };
    }

    void onWrite() {
        if (!handshakeDone) {
            if (socket_.written.find("\r\n\r\n") == std::string::npos) return;
            handshake = socket_.written;
            socket_.written.clear();
            handshakeDone = true;
            socket_.push(handshakeAnswer);
            return;
        }
        for (const ClientFrame& frame : takeClientFrames(socket_.written)) {
            if (frame.opcode != 0x1) continue;
            JsonDocument doc;
            REQUIRE(!deserializeJson(doc, frame.payload));
            messages.push_back(frame.payload);
            const std::string type = doc["type"] | "";
            if (type != "HMIP_SYSTEM_REQUEST") continue;
            const std::string id = doc["id"] | "";
            const std::string path = doc["body"]["path"] | "";
            paths.push_back(path);
            if (!answer) continue;
            std::string body;
            if (path == "/hmip/home/getSystemState") {
                body = "{\"code\":200,\"body\":" + readFixture("hcu_system_state.json") + "}";
            } else {
                body = "{\"code\":" + std::to_string(controlCode) + "}";
            }
            if (!eventBeforeAnswer.empty()) socket_.push(serverFrame(eventBeforeAnswer));
            socket_.push(serverFrame("{\"pluginId\":\"p\",\"id\":\"" + id +
                                     "\",\"type\":\"HMIP_SYSTEM_RESPONSE\",\"body\":" + body + "}"));
        }
    }

    JsonDocument lastRequestBody() {
        JsonDocument doc;
        deserializeJson(doc, messages.back());
        JsonDocument body;
        body.set(doc["body"]["body"]);
        return body;
    }

    std::string handshakeAnswer = kSwitchingProtocols;
    bool handshakeDone = false;
    bool answer = true;
    int controlCode = 200;
    std::string eventBeforeAnswer;
    std::string handshake;
    std::vector<std::string> messages;
    std::vector<std::string> paths;

private:
    FakeSocket& socket_;
};

hcu::HcuConfig makeConfig() {
    hcu::HcuConfig config;
    config.host = "192.168.178.50";
    config.authToken = "B23FB0DC";
    config.pluginId = "de.esp32sonoshub.display.test";
    return config;
}

std::string deviceEvent(const std::string& deviceJson) {
    return "{\"pluginId\":\"p\",\"id\":\"e1\",\"type\":\"HMIP_SYSTEM_EVENT\",\"body\":{\"eventTransaction\":{"
           "\"accessPointId\":\"x\",\"timestamp\":1,\"events\":{\"0\":" +
           deviceJson + "}}}}";
}

struct Fixture {
    FakeSocket socket;
    FakeHcu hcu{socket};
    FakeClock clock;
    hcu::HcuClient client{socket, clock, makeConfig(), 42};

    const Device& device(const std::string& name) {
        for (const Device& d : client.devices()) {
            if (d.name == name) return d;
        }
        FAIL("no device " << name);
        return client.devices().front();
    }
};

} // namespace

TEST_CASE("HCU connect sends credentials, reports READY and loads devices") {
    Fixture f;
    REQUIRE(f.client.connect() == HomeStatus::Ok);
    CHECK(f.client.isConnected());
    CHECK(f.socket.lastPort == 9001);
    CHECK(f.hcu.handshake.find("authtoken: B23FB0DC\r\n") != std::string::npos);
    CHECK(f.hcu.handshake.find("plugin-id: de.esp32sonoshub.display.test\r\n") != std::string::npos);
    CHECK(f.hcu.handshake.find("hmip-system-events: true\r\n") != std::string::npos);

    REQUIRE(f.hcu.messages.size() == 2);
    JsonDocument state;
    deserializeJson(state, f.hcu.messages[0]);
    CHECK(std::string(state["type"] | "") == "PLUGIN_STATE_RESPONSE");
    CHECK(std::string(state["pluginId"] | "") == "de.esp32sonoshub.display.test");
    CHECK(std::string(state["body"]["pluginReadinessStatus"] | "") == "READY");
    CHECK(std::string(state["id"] | "").size() == 36);
    CHECK(f.hcu.paths == std::vector<std::string>{"/hmip/home/getSystemState"});

    std::vector<std::string> names;
    for (const Device& d : f.client.devices()) names.push_back(d.name);
    CHECK(names == std::vector<std::string>{"Aktor Flur 1", "Flurlicht", "Kaffeemaschine", "Markise",
                                            "Rollladen Wohnzimmer", "Stehlampe"});

    const Device& shutter = f.device("Rollladen Wohnzimmer");
    CHECK(shutter.kind == DeviceKind::Shutter);
    CHECK(shutter.shutterControl == ShutterControl::ShutterLevel);
    CHECK(shutter.deviceId == "3014F711A0000000000000B1");
    CHECK(shutter.channel == 1);
    CHECK(shutter.level == doctest::Approx(0.25));
    CHECK_FALSE(shutter.moving);

    const Device& awning = f.device("Markise");
    CHECK(awning.shutterControl == ShutterControl::PrimaryShadingLevel);
    CHECK(awning.level == doctest::Approx(1.0));
    CHECK(awning.moving);

    CHECK(f.device("Stehlampe").kind == DeviceKind::Switch);
    CHECK(f.device("Stehlampe").on);
    CHECK_FALSE(f.device("Aktor Flur 1").on);
    CHECK(f.device("Flurlicht").channel == 2);
    CHECK_FALSE(f.device("Kaffeemaschine").reachable);
    CHECK(f.device("Stehlampe").reachable);
}

TEST_CASE("HCU rejected token") {
    Fixture f;
    f.hcu.handshakeAnswer = "HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\n\r\n";
    CHECK(f.client.connect() == HomeStatus::AuthError);
    CHECK_FALSE(f.client.isConnected());
}

TEST_CASE("HCU unreachable") {
    Fixture f;
    f.socket.acceptConnect = false;
    CHECK(f.client.connect() == HomeStatus::NetworkError);
}

TEST_CASE("HCU without token does not try to connect") {
    FakeSocket socket;
    FakeClock clock;
    hcu::HcuConfig config = makeConfig();
    config.authToken.clear();
    hcu::HcuClient client(socket, clock, config, 1);
    CHECK(client.connect() == HomeStatus::AuthError);
    CHECK(socket.connects == 0);
}

TEST_CASE("HCU control requests") {
    Fixture f;
    REQUIRE(f.client.connect() == HomeStatus::Ok);

    CHECK(f.client.setSwitch(f.device("Flurlicht"), false) == HomeStatus::Ok);
    CHECK(f.hcu.paths.back() == "/hmip/device/control/setSwitchState");
    JsonDocument body = f.hcu.lastRequestBody();
    CHECK(std::string(body["deviceId"] | "") == "3014F711A0000000000000S2");
    CHECK(body["channelIndex"] == 2);
    CHECK(body["on"] == false);

    CHECK(f.client.setShutterLevel(f.device("Rollladen Wohnzimmer"), 1.0) == HomeStatus::Ok);
    CHECK(f.hcu.paths.back() == "/hmip/device/control/setShutterLevel");
    body = f.hcu.lastRequestBody();
    CHECK(body["shutterLevel"] == 1.0);
    CHECK(body["channelIndex"] == 1);

    CHECK(f.client.setShutterLevel(f.device("Markise"), 0.0) == HomeStatus::Ok);
    CHECK(f.hcu.paths.back() == "/hmip/device/control/setPrimaryShadingLevel");
    CHECK(f.hcu.lastRequestBody()["primaryShadingLevel"] == 0.0);

    CHECK(f.client.stopShutter(f.device("Markise")) == HomeStatus::Ok);
    CHECK(f.hcu.paths.back() == "/hmip/device/control/stop");
    body = f.hcu.lastRequestBody();
    CHECK(std::string(body["deviceId"] | "") == "3014F711A0000000000000H1");

    f.hcu.controlCode = 400;
    CHECK(f.client.setSwitch(f.device("Stehlampe"), false) == HomeStatus::Rejected);
    CHECK(f.client.isConnected());
}

TEST_CASE("HCU request without answer drops the connection") {
    Fixture f;
    REQUIRE(f.client.connect() == HomeStatus::Ok);
    f.hcu.answer = false;
    CHECK(f.client.setSwitch(f.device("Stehlampe"), false) == HomeStatus::NetworkError);
    CHECK_FALSE(f.client.isConnected());
    CHECK(f.client.poll(10) == HomeStatus::NetworkError);
}

TEST_CASE("HCU events update, add and remove devices") {
    Fixture f;
    REQUIRE(f.client.connect() == HomeStatus::Ok);

    // Shutter starts moving
    f.socket.push(serverFrame(deviceEvent(
        "{\"pushEventType\":\"DEVICE_CHANGED\",\"device\":{\"id\":\"3014F711A0000000000000B1\","
        "\"label\":\"Rollladen Wohnzimmer\",\"functionalChannels\":{"
        "\"0\":{\"functionalChannelType\":\"DEVICE_BASE\",\"index\":0,\"unreach\":false},"
        "\"1\":{\"functionalChannelType\":\"SHUTTER_CHANNEL\",\"label\":\"\",\"index\":1,"
        "\"shutterLevel\":0.6,\"processing\":true}}}}")));
    CHECK(f.client.poll(10) == HomeStatus::Ok);
    CHECK(f.device("Rollladen Wohnzimmer").level == doctest::Approx(0.6));
    CHECK(f.device("Rollladen Wohnzimmer").moving);

    // New socket, renamed on the way; one device removed
    f.socket.push(serverFrame(deviceEvent(
        "{\"pushEventType\":\"DEVICE_ADDED\",\"device\":{\"id\":\"3014F711A0000000000000N1\","
        "\"label\":\"Bügeleisen\",\"functionalChannels\":{"
        "\"1\":{\"functionalChannelType\":\"SWITCH_MEASURING_CHANNEL\",\"index\":1,\"on\":true}}}}")));
    f.socket.push(serverFrame(deviceEvent(
        "{\"pushEventType\":\"DEVICE_REMOVED\",\"id\":\"3014F711A0000000000000S2\"}")));
    CHECK(f.client.poll(10) == HomeStatus::Ok);
    CHECK(f.client.devices().front().name == "Bügeleisen");
    CHECK(f.client.devices().front().on);
    for (const Device& d : f.client.devices()) CHECK(d.deviceId != "3014F711A0000000000000S2");
    CHECK(f.client.devices().size() == 5);

    // Nothing pending: poll just times out
    CHECK(f.client.poll(10) == HomeStatus::Ok);
    CHECK(f.client.devices().size() == 5);
}

TEST_CASE("HCU events arriving while waiting for a response are applied") {
    Fixture f;
    REQUIRE(f.client.connect() == HomeStatus::Ok);
    f.hcu.eventBeforeAnswer = deviceEvent(
        "{\"pushEventType\":\"DEVICE_CHANGED\",\"device\":{\"id\":\"3014F711A0000000000000P1\","
        "\"label\":\"Stehlampe\",\"functionalChannels\":{"
        "\"1\":{\"functionalChannelType\":\"SWITCH_MEASURING_CHANNEL\",\"index\":1,\"on\":false}}}}");
    CHECK(f.client.setSwitch(f.device("Stehlampe"), false) == HomeStatus::Ok);
    CHECK_FALSE(f.device("Stehlampe").on);
}

TEST_CASE("HCU answers plugin state requests") {
    Fixture f;
    REQUIRE(f.client.connect() == HomeStatus::Ok);
    f.socket.push(serverFrame("{\"pluginId\":\"p\",\"id\":\"req-7\",\"type\":\"PLUGIN_STATE_REQUEST\",\"body\":{}}"));
    CHECK(f.client.poll(10) == HomeStatus::Ok);
    JsonDocument doc;
    deserializeJson(doc, f.hcu.messages.back());
    CHECK(std::string(doc["type"] | "") == "PLUGIN_STATE_RESPONSE");
    CHECK(std::string(doc["id"] | "") == "req-7");
    CHECK(std::string(doc["body"]["pluginReadinessStatus"] | "") == "READY");
}

TEST_CASE("HCU connection closed by the server") {
    Fixture f;
    REQUIRE(f.client.connect() == HomeStatus::Ok);
    f.socket.push(serverFrame("", 0x8));
    CHECK(f.client.poll(10) == HomeStatus::NetworkError);
    CHECK_FALSE(f.client.isConnected());
}

TEST_CASE("HCU pairing exchanges the activation key for a confirmed token") {
    FakeSocket socket;
    socket.closeWhenEmpty = true;
    std::vector<std::string> requests;
    socket.onWrite = [&] {
        requests.push_back(socket.written);
        socket.written.clear();
        if (requests.size() == 1) {
            socket.push("HTTP/1.1 200 OK\r\nContent-Length: 21\r\n\r\n{\"authToken\":\"T0K3N\"}");
        } else {
            socket.push("HTTP/1.1 200 OK\r\nContent-Length: 16\r\n\r\n{\"clientId\":\"c\"}");
        }
    };
    const hcu::PairingResult result =
        hcu::pairWithHcu(socket, "hcu1-1234.local", "ABCDEF", "de.esp32sonoshub.display.test", "Sonos-Display");
    CHECK(result.ok);
    CHECK(result.authToken == "T0K3N");
    CHECK(socket.lastPort == 6969);
    REQUIRE(requests.size() == 2);
    CHECK(requests[0].rfind("POST /hmip/auth/requestConnectApiAuthToken HTTP/1.1\r\n", 0) == 0);
    CHECK(requests[0].find("VERSION: 12\r\n") != std::string::npos);
    CHECK(requests[0].find("\"activationKey\":\"ABCDEF\"") != std::string::npos);
    CHECK(requests[0].find("\"pluginId\":\"de.esp32sonoshub.display.test\"") != std::string::npos);
    CHECK(requests[0].find("\"de\":\"Sonos-Display\"") != std::string::npos);
    CHECK(requests[1].rfind("POST /hmip/auth/confirmConnectApiAuthToken HTTP/1.1\r\n", 0) == 0);
    CHECK(requests[1].find("\"authToken\":\"T0K3N\"") != std::string::npos);
}

TEST_CASE("HCU pairing with an expired activation key") {
    FakeSocket socket;
    socket.closeWhenEmpty = true;
    socket.onWrite = [&] {
        socket.push("HTTP/1.1 403 Forbidden\r\nContent-Length: 36\r\n\r\n{\"errorCode\":\"INVALID_ACTIVATION_K\"}");
    };
    const hcu::PairingResult result = hcu::pairWithHcu(socket, "h", "OLD", "p", "x");
    CHECK_FALSE(result.ok);
    CHECK(result.error.find("HTTP 403") != std::string::npos);
}
