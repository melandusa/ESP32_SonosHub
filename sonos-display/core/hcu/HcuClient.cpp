#include "hcu/HcuClient.h"

#include "hcu/SocketHttp.h"
#include "platform/Log.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace hcu {

using home::Device;
using home::DeviceKind;
using home::HomeStatus;
using home::ShutterControl;

namespace {

constexpr int kAuthPort = 6969;
constexpr int kPairingTimeoutMs = 10000;
constexpr const char* kApiVersion = "12";

struct ChannelType {
    const char* name;
    DeviceKind kind;
    ShutterControl shutterControl;
};

constexpr ChannelType kChannelTypes[] = {
    {"SWITCH_CHANNEL", DeviceKind::Switch, ShutterControl::ShutterLevel},
    {"SWITCH_MEASURING_CHANNEL", DeviceKind::Switch, ShutterControl::ShutterLevel},
    {"SHUTTER_CHANNEL", DeviceKind::Shutter, ShutterControl::ShutterLevel},
    {"BLIND_CHANNEL", DeviceKind::Shutter, ShutterControl::ShutterLevel},
    {"SHADING_CHANNEL", DeviceKind::Shutter, ShutterControl::PrimaryShadingLevel},
};

const ChannelType* findChannelType(const char* name) {
    if (!name) return nullptr;
    for (const ChannelType& type : kChannelTypes) {
        if (std::strcmp(type.name, name) == 0) return &type;
    }
    return nullptr;
}

// Only the parts of a device object the display needs; keeps the parsed system state small.
void addDeviceFilter(JsonObject device) {
    device["id"] = true;
    device["label"] = true;
    JsonObject channel = device["functionalChannels"]["*"].to<JsonObject>();
    for (const char* field : {"functionalChannelType", "label", "index", "on", "shutterLevel",
                              "primaryShadingLevel", "processing", "unreach"}) {
        channel[field] = true;
    }
}

std::string jsonString(JsonVariantConst value) {
    const char* text = value.as<const char*>();
    return text ? text : "";
}

void setTarget(JsonDocument& body, const Device& device) {
    body["deviceId"] = device.deviceId;
    body["channelIndex"] = device.channel;
}

} // namespace

void sortDevices(home::DeviceList& devices) {
    std::stable_sort(devices.begin(), devices.end(), [](const Device& a, const Device& b) {
        if (a.name != b.name) return a.name < b.name;
        if (a.deviceId != b.deviceId) return a.deviceId < b.deviceId;
        return a.channel < b.channel;
    });
}

void removeDevice(const std::string& deviceId, home::DeviceList& devices) {
    devices.erase(std::remove_if(devices.begin(), devices.end(),
                                 [&](const Device& d) { return d.deviceId == deviceId; }),
                  devices.end());
}

void applyDevice(JsonObjectConst device, home::DeviceList& devices) {
    const std::string id = jsonString(device["id"]);
    if (id.empty()) return;
    removeDevice(id, devices);

    const std::string label = jsonString(device["label"]);
    JsonObjectConst channels = device["functionalChannels"];
    bool reachable = true;
    int controllable = 0;
    for (JsonPairConst entry : channels) {
        JsonObjectConst channel = entry.value();
        if (channel["unreach"] | false) reachable = false;
        if (findChannelType(channel["functionalChannelType"])) ++controllable;
    }

    for (JsonPairConst entry : channels) {
        JsonObjectConst channel = entry.value();
        const ChannelType* type = findChannelType(channel["functionalChannelType"]);
        if (!type) continue;
        Device d;
        d.deviceId = id;
        d.channel = channel["index"] | std::atoi(entry.key().c_str());
        d.kind = type->kind;
        d.shutterControl = type->shutterControl;
        d.reachable = reachable;
        const std::string channelLabel = jsonString(channel["label"]);
        if (!channelLabel.empty()) {
            d.name = channelLabel;
        } else if (controllable > 1) {
            d.name = label + " " + std::to_string(d.channel);
        } else {
            d.name = label;
        }
        if (d.name.empty()) d.name = id;
        if (d.kind == DeviceKind::Switch) {
            d.on = channel["on"] | false;
        } else {
            JsonVariantConst level = channel["shutterLevel"];
            if (level.isNull() || d.shutterControl == ShutterControl::PrimaryShadingLevel) {
                if (!channel["primaryShadingLevel"].isNull()) level = channel["primaryShadingLevel"];
            }
            d.level = level.is<double>() ? level.as<double>() : -1.0;
            d.moving = channel["processing"] | false;
        }
        devices.push_back(std::move(d));
    }
    sortDevices(devices);
}

HcuClient::HcuClient(platform::IStreamSocket& socket, const platform::IClock& clock, HcuConfig config,
                     uint32_t randomSeed, ArduinoJson::Allocator* allocator)
    : socket_(socket),
      clock_(clock),
      config_(std::move(config)),
      randomState_(randomSeed ? randomSeed : 0x9E3779B9u),
      allocator_(allocator ? allocator : ArduinoJson::detail::DefaultAllocator::instance()),
      ws_(socket, [this] { return random(); }),
      filter_(allocator_) {
    filter_["type"] = true;
    filter_["id"] = true;
    filter_["body"]["code"] = true;
    filter_["body"]["body"]["errorCode"] = true;
    addDeviceFilter(filter_["body"]["body"]["devices"]["*"].to<JsonObject>());
    JsonObject event = filter_["body"]["eventTransaction"]["events"]["*"].to<JsonObject>();
    event["pushEventType"] = true;
    event["id"] = true;
    addDeviceFilter(event["device"].to<JsonObject>());
}

uint32_t HcuClient::random() {
    // xorshift32: only for message ids and WebSocket masks, no security relevance
    uint32_t x = randomState_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    randomState_ = x;
    return x;
}

std::string HcuClient::nextId() {
    // Random UUID (version 4 layout)
    uint32_t r[4] = {random(), random(), random(), random()};
    r[1] = (r[1] & 0xFFFF0FFFu) | 0x00004000u;
    r[2] = (r[2] & 0x3FFFFFFFu) | 0x80000000u;
    char text[40];
    std::snprintf(text, sizeof(text), "%08x-%04x-%04x-%04x-%04x%08x", static_cast<unsigned>(r[0]),
                  static_cast<unsigned>(r[1] >> 16), static_cast<unsigned>(r[1] & 0xFFFF),
                  static_cast<unsigned>(r[2] >> 16), static_cast<unsigned>(r[2] & 0xFFFF),
                  static_cast<unsigned>(r[3]));
    return text;
}

bool HcuClient::send(const char* type, const std::string& id, JsonDocument& body) {
    JsonDocument message(allocator_);
    message["pluginId"] = config_.pluginId;
    message["id"] = id;
    message["type"] = type;
    message["body"] = body.as<JsonObjectConst>();
    if (message["body"].isNull()) message["body"].to<JsonObject>();
    std::string text;
    serializeJson(message, text);
    LOGD("HCU > %s", text.c_str());
    return ws_.sendText(text);
}

HomeStatus HcuClient::connect() {
    ws_.close();
    if (config_.host.empty() || config_.authToken.empty()) return HomeStatus::AuthError;

    const HeaderList headers = {
        {"authtoken", config_.authToken},
        {"plugin-id", config_.pluginId},
        {"hmip-system-events", "true"},
    };
    if (!ws_.open(config_.host, config_.port, "/", headers, kConnectTimeoutMs)) {
        const int status = ws_.httpStatus();
        if (status == 401 || status == 403) {
            LOGW("HCU: authorization token rejected (HTTP %d)", status);
            return HomeStatus::AuthError;
        }
        LOGW("HCU: no connection to %s:%d", config_.host.c_str(), config_.port);
        return HomeStatus::NetworkError;
    }
    LOGI("HCU: connected to %s:%d", config_.host.c_str(), config_.port);

    // Report the plugin as running (shown on HCUweb), then load the system state.
    JsonDocument state(allocator_);
    state["pluginReadinessStatus"] = "READY";
    state["friendlyName"]["de"] = "Sonos-Display";
    state["friendlyName"]["en"] = "Sonos display";
    if (!send("PLUGIN_STATE_RESPONSE", nextId(), state)) return HomeStatus::NetworkError;

    JsonDocument empty(allocator_);
    devices_.clear();
    const HomeStatus status = request("/hmip/home/getSystemState", empty, kSystemStateTimeoutMs);
    if (status == HomeStatus::Ok) {
        int switches = 0;
        for (const Device& d : devices_) switches += d.kind == DeviceKind::Switch ? 1 : 0;
        LOGI("HCU: %d switches, %d shutters", switches, static_cast<int>(devices_.size()) - switches);
    } else if (status == HomeStatus::Rejected) {
        ws_.close();
        return HomeStatus::AuthError;  // e.g. CLIENT_ACCESS_DENIED
    }
    return status;
}

HomeStatus HcuClient::poll(int timeoutMs) {
    if (!ws_.isOpen()) return HomeStatus::NetworkError;
    // Handle everything that arrived, but wait at most timeoutMs for the first message.
    int waitMs = timeoutMs;
    for (int i = 0; i < 20; ++i) {
        if (!receive(waitMs, nullptr)) return ws_.isOpen() ? HomeStatus::Ok : HomeStatus::NetworkError;
        waitMs = 0;
    }
    return HomeStatus::Ok;
}

bool HcuClient::receive(int timeoutMs, Awaited* awaited) {
    std::string text;
    switch (ws_.readMessage(text, timeoutMs)) {
    case WebSocketClient::ReadStatus::Message: handleMessage(text, awaited); return true;
    case WebSocketClient::ReadStatus::Timeout: return false;
    case WebSocketClient::ReadStatus::Closed:
        LOGW("HCU: connection closed");
        return false;
    }
    return false;
}

void HcuClient::handleMessage(const std::string& text, Awaited* awaited) {
    JsonDocument doc(allocator_);
    const DeserializationError error = deserializeJson(doc, text, DeserializationOption::Filter(filter_),
                                                       DeserializationOption::NestingLimit(16));
    if (error) {
        LOGW("HCU: invalid message (%s, %u bytes)", error.c_str(), static_cast<unsigned>(text.size()));
        return;
    }
    const char* type = doc["type"] | "";
    const std::string id = jsonString(doc["id"]);
    LOGD("HCU < %s (%u bytes)", type, static_cast<unsigned>(text.size()));

    if (std::strcmp(type, "HMIP_SYSTEM_RESPONSE") == 0) {
        if (!awaited || id != awaited->id) return;  // late answer to an abandoned request
        awaited->code = doc["body"]["code"] | 500;
        JsonObjectConst devices = doc["body"]["body"]["devices"];
        if (!devices.isNull()) {
            devices_.clear();
            for (JsonPairConst entry : devices) applyDevice(entry.value(), devices_);
        }
        if (awaited->code != 200) {
            LOGW("HCU: request failed, code %d %s", awaited->code, doc["body"]["body"]["errorCode"] | "");
        }
    } else if (std::strcmp(type, "HMIP_SYSTEM_EVENT") == 0) {
        JsonObjectConst events = doc["body"]["eventTransaction"]["events"];
        for (JsonPairConst entry : events) {
            JsonObjectConst event = entry.value();
            const char* pushType = event["pushEventType"] | "";
            if (std::strcmp(pushType, "DEVICE_CHANGED") == 0 || std::strcmp(pushType, "DEVICE_ADDED") == 0) {
                applyDevice(event["device"], devices_);
            } else if (std::strcmp(pushType, "DEVICE_REMOVED") == 0) {
                removeDevice(jsonString(event["id"]), devices_);
            }
        }
    } else if (std::strcmp(type, "PLUGIN_STATE_REQUEST") == 0) {
        JsonDocument state(allocator_);
        state["pluginReadinessStatus"] = "READY";
        send("PLUGIN_STATE_RESPONSE", id, state);
    }
}

HomeStatus HcuClient::request(const std::string& path, JsonDocument& body, int timeoutMs) {
    if (!ws_.isOpen()) return HomeStatus::NetworkError;
    Awaited awaited;
    awaited.id = nextId();
    JsonDocument wrapper(allocator_);
    wrapper["path"] = path;
    wrapper["body"] = body.as<JsonObjectConst>();
    if (wrapper["body"].isNull()) wrapper["body"].to<JsonObject>();
    if (!send("HMIP_SYSTEM_REQUEST", awaited.id, wrapper)) return HomeStatus::NetworkError;

    const uint64_t deadline = clock_.nowMs() + static_cast<uint64_t>(timeoutMs);
    while (awaited.code == 0) {
        const uint64_t now = clock_.nowMs();
        if (now >= deadline || !ws_.isOpen()) {
            LOGW("HCU: no answer to %s", path.c_str());
            ws_.close();  // the connection is in an unknown state: start over
            return HomeStatus::NetworkError;
        }
        receive(static_cast<int>(std::min<uint64_t>(deadline - now, 1000)), &awaited);
    }
    return awaited.code == 200 ? HomeStatus::Ok : HomeStatus::Rejected;
}

HomeStatus HcuClient::setSwitch(const Device& device, bool on) {
    JsonDocument body(allocator_);
    setTarget(body, device);
    body["on"] = on;
    LOGI("HCU: %s %s", device.name.c_str(), on ? "on" : "off");
    return request("/hmip/device/control/setSwitchState", body, kResponseTimeoutMs);
}

HomeStatus HcuClient::setShutterLevel(const Device& device, double level) {
    JsonDocument body(allocator_);
    setTarget(body, device);
    level = std::clamp(level, 0.0, 1.0);
    LOGI("HCU: %s to level %.2f", device.name.c_str(), level);
    if (device.shutterControl == ShutterControl::PrimaryShadingLevel) {
        body["primaryShadingLevel"] = level;
        return request("/hmip/device/control/setPrimaryShadingLevel", body, kResponseTimeoutMs);
    }
    body["shutterLevel"] = level;
    return request("/hmip/device/control/setShutterLevel", body, kResponseTimeoutMs);
}

HomeStatus HcuClient::stopShutter(const Device& device) {
    JsonDocument body(allocator_);
    setTarget(body, device);
    LOGI("HCU: stop %s", device.name.c_str());
    return request("/hmip/device/control/stop", body, kResponseTimeoutMs);
}

PairingResult pairWithHcu(platform::IStreamSocket& socket, const std::string& host,
                          const std::string& activationKey, const std::string& pluginId,
                          const std::string& displayName) {
    PairingResult result;
    const HeaderList headers = {{"VERSION", kApiVersion}};
    auto describe = [](const platform::HttpResponse& response) {
        if (response.status == 0) return std::string("keine Verbindung");
        JsonDocument doc;
        std::string text = "HTTP " + std::to_string(response.status);
        if (!deserializeJson(doc, response.body) && doc["errorCode"].is<const char*>()) {
            text += std::string(" ") + doc["errorCode"].as<const char*>();
        }
        return text;
    };

    JsonDocument requestBody;
    requestBody["activationKey"] = activationKey;
    requestBody["pluginId"] = pluginId;
    requestBody["friendlyName"]["de"] = displayName;
    requestBody["friendlyName"]["en"] = displayName;
    std::string body;
    serializeJson(requestBody, body);
    const platform::HttpResponse requested = postOverSocket(
        socket, host, kAuthPort, "/hmip/auth/requestConnectApiAuthToken", headers, body, kPairingTimeoutMs);
    JsonDocument answer;
    if (requested.status != 200 || deserializeJson(answer, requested.body) ||
        !answer["authToken"].is<const char*>()) {
        result.error = "Token anfordern: " + describe(requested);
        return result;
    }
    const std::string token = answer["authToken"].as<const char*>();

    JsonDocument confirmBody;
    confirmBody["activationKey"] = activationKey;
    confirmBody["authToken"] = token;
    body.clear();
    serializeJson(confirmBody, body);
    const platform::HttpResponse confirmed = postOverSocket(
        socket, host, kAuthPort, "/hmip/auth/confirmConnectApiAuthToken", headers, body, kPairingTimeoutMs);
    if (confirmed.status != 200) {
        result.error = "Token bestätigen: " + describe(confirmed);
        return result;
    }
    result.ok = true;
    result.authToken = token;
    return result;
}

} // namespace hcu
