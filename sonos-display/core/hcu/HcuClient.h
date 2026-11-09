#pragma once

#include "hcu/WebSocketClient.h"
#include "home/IHomeBackend.h"
#include "platform/IClock.h"
#include "platform/IStreamSocket.h"

#include <ArduinoJson.h>

#include <cstdint>
#include <string>

namespace hcu {

struct HcuConfig {
    std::string host;  // IP address or hcu1-XXXX.local
    int port = 9001;   // Connect API WebSocket
    std::string authToken;
    std::string pluginId;  // must match the one used when pairing
};

// Talks to the Homematic IP Home Control Unit over the Connect API WebSocket
// (developer mode, "expose WebSocket" switched on). Keeps the switches and shutters of the
// system state up to date from HMIP_SYSTEM_EVENTs. Worker thread only.
class HcuClient : public home::IHomeBackend {
public:
    // allocator: memory for parsed JSON (ESP32: PSRAM); nullptr = default heap.
    HcuClient(platform::IStreamSocket& socket, const platform::IClock& clock, HcuConfig config,
              uint32_t randomSeed, ArduinoJson::Allocator* allocator = nullptr);

    home::HomeStatus connect() override;
    bool isConnected() const override { return ws_.isOpen(); }
    home::HomeStatus poll(int timeoutMs) override;
    home::HomeStatus setSwitch(const home::Device& device, bool on) override;
    home::HomeStatus setShutterLevel(const home::Device& device, double level) override;
    home::HomeStatus stopShutter(const home::Device& device) override;
    const home::DeviceList& devices() const override { return devices_; }

    static constexpr int kConnectTimeoutMs = 8000;
    static constexpr int kResponseTimeoutMs = 8000;
    static constexpr int kSystemStateTimeoutMs = 20000;

private:
    struct Awaited {
        std::string id;
        int code = 0;  // HTTP-like result code once the response arrived
    };

    // Sends an HMIP_SYSTEM_REQUEST and waits for its response, handling events meanwhile.
    home::HomeStatus request(const std::string& path, JsonDocument& body, int timeoutMs);
    bool send(const char* type, const std::string& id, JsonDocument& body);
    // Reads and handles one message; false if the connection broke.
    bool receive(int timeoutMs, Awaited* awaited);
    void handleMessage(const std::string& text, Awaited* awaited);
    std::string nextId();
    uint32_t random();

    platform::IStreamSocket& socket_;
    const platform::IClock& clock_;
    HcuConfig config_;
    uint32_t randomState_;
    ArduinoJson::Allocator* allocator_;
    WebSocketClient ws_;
    home::DeviceList devices_;
    JsonDocument filter_;
};

// Applies one device object of the system state: replaces all channels of that device
// in `devices` (sorted by name afterwards). Exposed for tests.
void applyDevice(JsonObjectConst device, home::DeviceList& devices);
void removeDevice(const std::string& deviceId, home::DeviceList& devices);
void sortDevices(home::DeviceList& devices);

struct PairingResult {
    bool ok = false;
    std::string authToken;
    std::string error;  // for the log / display if !ok
};

// One-time pairing: exchanges an activation key from HCUweb (developer mode page) for an
// authorization token bound to pluginId. Port 6969, HTTPS with the HCU's self-signed certificate.
PairingResult pairWithHcu(platform::IStreamSocket& socket, const std::string& host,
                          const std::string& activationKey, const std::string& pluginId,
                          const std::string& displayName);

} // namespace hcu
