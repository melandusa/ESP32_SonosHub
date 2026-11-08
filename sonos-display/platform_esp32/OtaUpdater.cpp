#include "OtaUpdater.h"

#include "app/UpdateManifest.h"
#include "platform/Log.h"

#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <Update.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>

// Mozilla's root certificates, embedded via board_build.embed_files (see certs/README.md)
extern const uint8_t kCaBundleStart[] asm("_binary_platform_esp32_certs_x509_crt_bundle_bin_start");
extern const uint8_t kCaBundleEnd[] asm("_binary_platform_esp32_certs_x509_crt_bundle_bin_end");

namespace esp_platform {

namespace {

constexpr uint32_t kTaskStackBytes = 12 * 1024;  // TLS handshakes need a large stack
constexpr int kTimeoutMs = 15000;

void useCaBundle(NetworkClientSecure& client) {
    client.setCACertBundle(kCaBundleStart, static_cast<size_t>(kCaBundleEnd - kCaBundleStart));
}

void configure(HTTPClient& http) {
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // GitHub redirects to its CDN
    http.setConnectTimeout(kTimeoutMs);
    http.setTimeout(kTimeoutMs);
}

void logRequestError(const char* what, int code, NetworkClientSecure& client) {
    char tlsError[100] = "";
    client.lastError(tlsError, sizeof(tlsError));
    LOGW("%s: HTTP %d (%s), TLS: %s, internal heap %u KB free, largest block %u KB", what, code,
         HTTPClient::errorToString(code).c_str(), tlsError,
         static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
         static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024));
}

} // namespace

OtaUpdater::OtaUpdater(std::string manifestUrl, int currentVersion)
    : manifestUrl_(std::move(manifestUrl)), currentVersion_(currentVersion) {}

void OtaUpdater::begin() {
    xTaskCreatePinnedToCore(taskMain, "ota", kTaskStackBytes, this, 1, &task_, 0);
}

void OtaUpdater::checkNow() {
    const State state = state_.load();
    if (state == State::Checking || state == State::Installing || state == State::Installed) return;
    request_ = Request::Check;
    state_ = State::Checking;
    xTaskNotifyGive(task_);
}

void OtaUpdater::install() {
    if (state_.load() != State::Available) return;
    request_ = Request::Install;
    progress_ = 0;
    state_ = State::Installing;
    xTaskNotifyGive(task_);
}

void OtaUpdater::taskMain(void* arg) {
    auto* self = static_cast<OtaUpdater*>(arg);
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        switch (self->request_.exchange(Request::None)) {
        case Request::Check: self->runCheck(); break;
        case Request::Install: self->runInstall(); break;
        case Request::None: break;
        }
    }
}

void OtaUpdater::runCheck() {
    NetworkClientSecure client;
    useCaBundle(client);
    HTTPClient http;
    configure(http);

    app::UpdateManifest manifest;
    bool ok = false;
    if (http.begin(client, manifestUrl_.c_str())) {
        const int code = http.GET();
        if (code == HTTP_CODE_OK) {
            ok = app::parseUpdateManifest(http.getString().c_str(), manifest);
            if (!ok) LOGW("Update check: unreadable version.json");
        } else {
            logRequestError("Update check", code, client);
        }
        http.end();
    }
    if (!ok) {
        state_ = State::Failed;
        return;
    }

    if (manifest.version <= currentVersion_) {
        LOGI("Update check: version %d is current", currentVersion_);
        state_ = State::Idle;
        return;
    }
    LOGI("Update check: version %d available (running %d)", manifest.version, currentVersion_);
    firmwareUrl_ = manifest.url;
    availableVersion_ = manifest.version;
    state_ = State::Available;
}

void OtaUpdater::runInstall() {
    LOGI("Update: downloading %s", firmwareUrl_.c_str());
    NetworkClientSecure client;
    useCaBundle(client);
    HTTPClient http;
    configure(http);
    http.useHTTP10(true);  // plain body with Content-Length, no chunked encoding to undo
    if (!http.begin(client, firmwareUrl_.c_str())) {
        state_ = State::Failed;
        return;
    }

    bool ok = false;
    const int code = http.GET();
    const int size = http.getSize();
    if (code != HTTP_CODE_OK || size <= 0) {
        logRequestError("Update download", code, client);
    } else if (!Update.begin(static_cast<size_t>(size))) {
        LOGW("Update: %s", Update.errorString());
    } else {
        // The new image goes into the unused OTA slot; the running firmware stays untouched,
        // so an aborted download or power loss is harmless. end() verifies the image.
        Update.onProgress([this](size_t done, size_t total) {
            if (total > 0) progress_ = static_cast<int>(uint64_t(done) * 100 / total);
        });
        const int written = static_cast<int>(Update.writeStream(*http.getStreamPtr()));
        if (written != size) {
            LOGW("Update: download incomplete (%d of %d bytes)", written, size);
            Update.abort();
        } else if (!Update.end()) {
            LOGW("Update: %s", Update.errorString());
        } else {
            ok = true;
        }
    }
    http.end();

    if (ok) {
        LOGI("Update: version %d installed", availableVersion_.load());
        state_ = State::Installed;
    } else {
        state_ = State::Failed;
    }
}

bool OtaUpdater::isPendingVerification() {
    esp_ota_img_states_t state;
    return esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
           state == ESP_OTA_IMG_PENDING_VERIFY;
}

void OtaUpdater::confirmFirmware() {
    if (!isPendingVerification()) return;
    LOGI("Update: new firmware confirmed");
    esp_ota_mark_app_valid_cancel_rollback();
}

void OtaUpdater::rejectFirmware() {
    LOGE("Update: new firmware did not reach the WLAN, returning to the previous version");
    esp_ota_mark_app_invalid_rollback_and_reboot();
}

} // namespace esp_platform

// Arduino would confirm a new firmware right at startup; we confirm it ourselves once the
// WLAN works (OtaUpdater::confirmFirmware).
extern "C" bool verifyRollbackLater() { return true; }
