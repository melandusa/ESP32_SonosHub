// Firmware entry for the Guition ESP32-S3-4848S040.
#include "app/AppController.h"
#include "app/SonosService.h"
#include "platform/Log.h"
#include "sonos/SonosClient.h"
#include "ui/Fonts.h"
#include "ui/LogoDecoder.h"
#include "ui/MainScreen.h"

#include "Board.h"
#include "Display.h"
#include "EspHttpClient.h"
#include "EspPlatform.h"
#include "FreeRtosWorker.h"
#include "WifiUdpSsdpDiscovery.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "lvgl.h"

namespace {

// TODO(settings): make these configurable (e.g. as WiFiManager parameters stored in NVS).
constexpr const char* kFixedSpeakerIp = "";  // empty: find the speaker via SSDP
constexpr int kMaxVolume = 60;

constexpr const char* kSetupNetworkName = "SonosDisplay-Setup";
constexpr platform::LogLevel kLogLevel = platform::LogLevel::Info;
constexpr uint32_t kUiTickMs = 30;

esp_platform::SerialLogger logger;
esp_platform::EspClock systemClock;
esp_platform::PwmBacklight backlight(board::kBacklight);
esp_platform::EspHttpClient http;
esp_platform::WifiUdpSsdpDiscovery discovery;
sonos::SonosClient client(http);
app::SonosService service(client, discovery, kFixedSpeakerIp);

esp_platform::FreeRtosWorker* worker = nullptr;
app::AppController* controller = nullptr;
ui::MainScreen* screen = nullptr;
WiFiManager wifiManager;
bool appStarted = false;

uint32_t tickMs() { return millis(); }

void logLvgl(lv_log_level_t /*level*/, const char* text) { Serial.print(text); }

void onUiTick(lv_timer_t* /*timer*/) {
    if (lv_display_get_inactive_time(nullptr) < kUiTickMs * 2) controller->noteUserActivity();

    if (!appStarted) {
        if (WiFi.status() == WL_CONNECTED) {
            LOGI("WLAN connected, IP %s", WiFi.localIP().toString().c_str());
            screen->setSystemMessage("");
            controller->start();
            appStarted = true;
        }
    } else {
        controller->tick();
    }
    screen->refresh();
}

} // namespace

void setup() {
    Serial.begin(115200);
    platform::setLogger(&logger);
    platform::setMinLogLevel(kLogLevel);
    LOGI("Sonos display starting, PSRAM %u KB free", static_cast<unsigned>(ESP.getFreePsram() / 1024));

    backlight.begin();
    backlight.setLevel(100);

    lv_init();
    lv_tick_set_cb(tickMs);
    lv_log_register_print_cb(logLvgl);
    if (!esp_platform::initDisplayAndTouch()) {
        LOGE("Display init failed, halting");
        while (true) delay(1000);
    }
    ui::fonts::init();

    app::AppConfig config;
    config.maxVolume = kMaxVolume;
    service.setLogoDecoder(ui::decodeLogo, config.logoSizePx);  // before the worker starts
    worker = new esp_platform::FreeRtosWorker(
        [](const app::Command& command) { return service.execute(command); });
    controller = new app::AppController(*worker, systemClock, backlight, config);
    screen = new ui::MainScreen(*controller);
    screen->create(lv_screen_active());
    screen->setSystemMessage(LV_SYMBOL_WIFI " WLAN verbindet…");
    lv_timer_handler();  // show the message before the (blocking) connect attempt

    // Saved credentials are tried first; without them a setup hotspot is opened.
    WiFi.mode(WIFI_STA);
    wifiManager.setConfigPortalBlocking(false);
    wifiManager.setConnectTimeout(15);
    wifiManager.setTitle("Sonos-Display");
    if (!wifiManager.autoConnect(kSetupNetworkName)) {
        LOGI("WLAN setup portal open: %s", kSetupNetworkName);
        screen->setSystemMessage(std::string(LV_SYMBOL_WIFI " WLAN einrichten: mit „") +
                                 kSetupNetworkName + "“ verbinden");
    }

    lv_timer_create(onUiTick, kUiTickMs, nullptr);
}

void loop() {
    wifiManager.process();
    const uint32_t sleepMs = lv_timer_handler();
    delay(sleepMs == LV_NO_TIMER_READY || sleepMs > 10 ? 10 : sleepMs);
}
