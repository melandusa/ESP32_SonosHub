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
#include "OtaUpdater.h"
#include "Settings.h"
#include "WifiUdpSsdpDiscovery.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include <cstdio>
#include <ctime>

#include "lvgl.h"

#ifndef SONOS_FIRMWARE_VERSION
#define SONOS_FIRMWARE_VERSION 0  // set from the file VERSION by firmware_version.py
#endif

namespace {

constexpr const char* kSetupNetworkName = "SonosDisplay-Setup";
constexpr platform::LogLevel kLogLevel = platform::LogLevel::Info;
constexpr uint32_t kUiTickMs = 30;

// Holding a finger on the screen this long during startup erases the WLAN credentials.
constexpr uint32_t kWifiResetHoldMs = 5000;

// Firmware updates: published by .github/workflows/release.yml
constexpr const char* kUpdateManifestUrl =
    "https://github.com/melandusa/ESP32_SonosHub/releases/latest/download/version.json";
constexpr uint32_t kFirstUpdateCheckDelayMs = 2 * 60 * 1000;
constexpr int kUpdateHour = 3;  // daily check at 03:00 local time
constexpr uint32_t kUpdateIntervalWithoutClockMs = 24 * 60 * 60 * 1000;
constexpr const char* kTimeZone = "CET-1CEST,M3.5.0,M10.5.0/3";  // Germany
// A freshly updated firmware that does not reach the WLAN in time is rolled back.
constexpr uint32_t kFirmwareProbationMs = 5 * 60 * 1000;

esp_platform::SerialLogger logger;
esp_platform::EspClock systemClock;
esp_platform::PwmBacklight backlight(board::kBacklight);
esp_platform::EspHttpClient http;
esp_platform::WifiUdpSsdpDiscovery discovery;
sonos::SonosClient client(http);
esp_platform::Settings settings;
esp_platform::OtaUpdater updater(kUpdateManifestUrl, SONOS_FIRMWARE_VERSION);

app::SonosService* service = nullptr;
esp_platform::FreeRtosWorker* worker = nullptr;
app::AppController* controller = nullptr;
ui::MainScreen* screen = nullptr;
WiFiManager wifiManager;
WiFiManagerParameter* speakerIpParam = nullptr;
WiFiManagerParameter* maxVolumeParam = nullptr;
bool appStarted = false;
bool restartAfterSetup = false;  // new settings from the portal take effect after a restart
bool updateMessageShown = false;
uint64_t nextUpdateCheckMs = 0;

uint32_t tickMs() { return millis(); }

void logLvgl(lv_log_level_t /*level*/, const char* text) { Serial.print(text); }

// Settings entered in the setup portal (shown below the WLAN password).
void onPortalSaved() {
    IPAddress ip;
    const String ipText = String(speakerIpParam->getValue());
    if (ipText.isEmpty()) {
        settings.speakerIp.clear();
    } else if (ip.fromString(ipText)) {
        settings.speakerIp = ip.toString().c_str();
    } else {
        LOGW("Setup: invalid speaker IP \"%s\" ignored", ipText.c_str());
    }
    const int maxVolume = atoi(maxVolumeParam->getValue());
    if (maxVolume >= 1 && maxVolume <= 100) {
        settings.maxVolume = maxVolume;
    } else {
        LOGW("Setup: invalid maximum volume \"%s\" ignored", maxVolumeParam->getValue());
    }
    settings.save();
    LOGI("Setup: speaker IP \"%s\", max volume %d", settings.speakerIp.c_str(), settings.maxVolume);
    restartAfterSetup = true;
}

// Waits while a finger stays on the screen; true if it stayed for kWifiResetHoldMs.
bool wifiResetRequested() {
    if (!esp_platform::isTouchPressed()) return false;
    const uint32_t start = millis();
    int shownSeconds = -1;
    while (esp_platform::isTouchPressed()) {
        const uint32_t held = millis() - start;
        if (held >= kWifiResetHoldMs) return true;
        const int seconds = static_cast<int>((kWifiResetHoldMs - held + 999) / 1000);
        if (seconds != shownSeconds) {
            shownSeconds = seconds;
            char text[96];
            std::snprintf(text, sizeof(text), LV_SYMBOL_WIFI " WLAN neu einrichten: weiter drücken (%d s)",
                          seconds);
            screen->setSystemMessage(text);
            lv_refr_now(nullptr);  // draw without processing the touch as a tap
        }
        delay(50);
    }
    return false;
}

// Milliseconds until the next daily update check, from the NTP clock if it is set.
uint32_t msUntilUpdateTime() {
    const time_t now = time(nullptr);
    if (now < 1700000000) return kUpdateIntervalWithoutClockMs;  // no NTP time yet
    tm local;
    localtime_r(&now, &local);
    tm target = local;
    target.tm_hour = kUpdateHour;
    target.tm_min = 0;
    target.tm_sec = 0;
    time_t next = mktime(&target);
    if (next <= now) {
        target.tm_mday += 1;
        next = mktime(&target);
    }
    return static_cast<uint32_t>(next - now) * 1000;
}

// Checks for new firmware after startup and daily, installs it only while nobody uses the
// display (screen off, no music), then restarts.
void handleFirmwareUpdate() {
    const uint64_t now = systemClock.nowMs();
    const esp_platform::OtaUpdater::State state = updater.state();
    using State = esp_platform::OtaUpdater::State;

    if (state == State::Installing) {
        char text[96];
        std::snprintf(text, sizeof(text), LV_SYMBOL_DOWNLOAD " Update wird installiert (%d %%) – bitte nicht ausstecken",
                      updater.progressPercent());
        screen->setSystemMessage(text);
        updateMessageShown = true;
        return;
    }
    if (state == State::Installed) {
        screen->setSystemMessage(LV_SYMBOL_OK " Update installiert – Neustart…");
        lv_refr_now(nullptr);
        delay(1500);
        ESP.restart();
    }
    if (updateMessageShown) {  // download failed: back to normal, retry at the next check
        screen->setSystemMessage("");
        updateMessageShown = false;
    }
    if (state == State::Checking) return;

    if (now >= nextUpdateCheckMs) {
        nextUpdateCheckMs = now + msUntilUpdateTime();
        updater.checkNow();
        return;
    }
    const sonos::TransportState transport = controller->state().transport;
    const bool musicPlaying = transport == sonos::TransportState::Playing ||
                              transport == sonos::TransportState::Transitioning;
    if (state == State::Available && controller->isScreenOff() && !musicPlaying) {
        LOGI("Update: installing version %d", updater.availableVersion());
        updater.install();
    }
}

void onUiTick(lv_timer_t* /*timer*/) {
    if (lv_display_get_inactive_time(nullptr) < kUiTickMs * 2) controller->noteUserActivity();

    if (!appStarted) {
        if (WiFi.status() == WL_CONNECTED) {
            LOGI("WLAN connected, IP %s", WiFi.localIP().toString().c_str());
            esp_platform::OtaUpdater::confirmFirmware();
            if (restartAfterSetup) {
                screen->setSystemMessage(LV_SYMBOL_OK " Gespeichert – Neustart…");
                lv_refr_now(nullptr);
                delay(1500);
                ESP.restart();
            }
            configTzTime(kTimeZone, "pool.ntp.org", "time.cloudflare.com");
            nextUpdateCheckMs = systemClock.nowMs() + kFirstUpdateCheckDelayMs;
            screen->setSystemMessage("");
            controller->start();
            appStarted = true;
        } else if (millis() > kFirmwareProbationMs && esp_platform::OtaUpdater::isPendingVerification()) {
            esp_platform::OtaUpdater::rejectFirmware();
        }
    } else {
        controller->tick();
        handleFirmwareUpdate();
    }
    screen->refresh();
}

} // namespace

void setup() {
    Serial.begin(115200);
    platform::setLogger(&logger);
    platform::setMinLogLevel(kLogLevel);
    LOGI("Sonos display version %d starting, PSRAM %u KB free", SONOS_FIRMWARE_VERSION,
         static_cast<unsigned>(ESP.getFreePsram() / 1024));
    if (esp_platform::OtaUpdater::isPendingVerification()) LOGI("Update: first start of new firmware");
    settings.load();

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
    config.maxVolume = settings.maxVolume;
    service = new app::SonosService(client, discovery, settings.speakerIp);
    service->setLogoDecoder(ui::decodeLogo, config.logoSizePx);  // before the worker starts
    worker = new esp_platform::FreeRtosWorker(
        [](const app::Command& command) { return service->execute(command); });
    controller = new app::AppController(*worker, systemClock, backlight, config);
    screen = new ui::MainScreen(*controller);
    screen->create(lv_screen_active());
    screen->setVersionText("Version " + std::to_string(SONOS_FIRMWARE_VERSION));

    WiFi.mode(WIFI_STA);
    if (wifiResetRequested()) {
        LOGI("WLAN credentials erased by touch at startup");
        wifiManager.resetSettings();
    }
    screen->setSystemMessage(LV_SYMBOL_WIFI " WLAN verbindet…");
    lv_refr_now(nullptr);  // show the message before the (blocking) connect attempt

    // Saved credentials are tried first; without them a setup hotspot is opened.
    char maxVolumeText[8];
    std::snprintf(maxVolumeText, sizeof(maxVolumeText), "%d", settings.maxVolume);
    speakerIpParam = new WiFiManagerParameter(
        "speakerip", "Lautsprecher-IP (leer = automatisch suchen)", settings.speakerIp.c_str(), 15);
    maxVolumeParam = new WiFiManagerParameter("maxvolume", "Maximale Lautstärke (1–100)", maxVolumeText, 3);
    wifiManager.addParameter(speakerIpParam);
    wifiManager.addParameter(maxVolumeParam);
    wifiManager.setSaveParamsCallback(onPortalSaved);
    wifiManager.setConfigPortalBlocking(false);
    wifiManager.setConnectTimeout(15);
    const std::string title = "Sonos-Display (Version " + std::to_string(SONOS_FIRMWARE_VERSION) + ")";
    wifiManager.setTitle(title.c_str());
    if (!wifiManager.autoConnect(kSetupNetworkName)) {
        LOGI("WLAN setup portal open: %s", kSetupNetworkName);
        screen->setSystemMessage(std::string(LV_SYMBOL_WIFI " WLAN einrichten: mit „") +
                                 kSetupNetworkName + "“ verbinden");
    }

    updater.begin();
    lv_timer_create(onUiTick, kUiTickMs, nullptr);
}

void loop() {
    wifiManager.process();
    const uint32_t sleepMs = lv_timer_handler();
    delay(sleepMs == LV_NO_TIMER_READY || sleepMs > 10 ? 10 : sleepMs);
}
