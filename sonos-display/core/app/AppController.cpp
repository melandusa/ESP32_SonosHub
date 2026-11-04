#include "app/AppController.h"

#include "platform/Log.h"

#include <algorithm>

namespace app {

AppController::AppController(IWorker& worker, const platform::IClock& clock,
                             platform::IBacklight& backlight, AppConfig config)
    : worker_(worker), clock_(clock), backlight_(backlight), config_(config),
      backoffMs_(config.reconnectBackoffMinMs) {}

void AppController::start() {
    const uint64_t now = clock_.nowMs();
    state_.connection = Connection::Connecting;
    nextDiscoveryMs_ = now;
    lastActivityMs_ = now;
    backlight_.setLevel(config_.backlightBright);
    markChanged();
}

void AppController::tick() {
    const uint64_t now = clock_.nowMs();

    Result result;
    while (worker_.poll(result)) handleResult(result, now);

    if (state_.connection != Connection::Connected) {
        if (!discoverInFlight_ && now >= nextDiscoveryMs_) {
            Command cmd;
            cmd.type = CommandType::Discover;
            cmd.preferredIp = lastDeviceIp_;
            discoverInFlight_ = submit(cmd) != 0;
        }
    } else {
        if (!statusInFlight_ && now >= nextPollMs_) {
            Command cmd;
            cmd.type = CommandType::RefreshStatus;
            statusInFlight_ = submit(cmd) != 0;
            nextPollMs_ = now + config_.pollIntervalMs;
        }
        if (!favoritesInFlight_ && now >= nextFavoritesMs_) {
            Command cmd;
            cmd.type = CommandType::LoadFavorites;
            favoritesInFlight_ = submit(cmd) != 0;
            nextFavoritesMs_ = now + config_.favoritesIntervalMs;
        }
        flushVolume(now);
        loadNextLogo(now);
    }

    updateDimming(now);
}

uint32_t AppController::submit(Command command) {
    command.seq = nextSeq_++;
    if (worker_.submit(command)) {
        ++commandsInFlight_;
        return command.seq;
    }
    LOGW("Worker queue full, dropping %s", commandName(command.type));
    return 0;
}

void AppController::handleResult(const Result& result, uint64_t now) {
    if (commandsInFlight_ > 0) --commandsInFlight_;
    switch (result.type) {
    case CommandType::Discover:
        discoverInFlight_ = false;
        handleDiscovery(result, now);
        return;  // discovery has its own backoff, no failure counting
    case CommandType::RefreshStatus:
        statusInFlight_ = false;
        if (result.ok()) handleStatus(result);
        break;
    case CommandType::LoadFavorites:
        favoritesInFlight_ = false;
        if (result.ok()) handleFavorites(result);
        break;
    case CommandType::PlayFavorite:
    case CommandType::Stop:
        // Show the real outcome soon instead of waiting for the next regular poll.
        nextPollMs_ = std::min(nextPollMs_, now + 300);
        break;
    case CommandType::SetVolume:
        break;
    case CommandType::LoadLogo:
        handleLogo(result, now);
        return;  // logos are optional, their errors say nothing about the speaker
    }
    trackFailures(result, now);
}

void AppController::handleDiscovery(const Result& result, uint64_t now) {
    if (result.ok()) {
        LOGI("Connected to \"%s\" at %s", result.roomName.c_str(), result.deviceIp.c_str());
        lastDeviceIp_ = result.deviceIp;
        state_.connection = Connection::Connected;
        state_.roomName = result.roomName;
        consecutiveFailures_ = 0;
        backoffMs_ = config_.reconnectBackoffMinMs;
        nextPollMs_ = now;
        nextFavoritesMs_ = now;
        markChanged();
        return;
    }

    LOGW("No Sonos speaker found, retrying in %u s", static_cast<unsigned>(backoffMs_ / 1000));
    if (state_.connection != Connection::Unreachable) {
        state_.connection = Connection::Unreachable;
        markChanged();
    }
    nextDiscoveryMs_ = now + backoffMs_;
    backoffMs_ = std::min(backoffMs_ * 2, config_.reconnectBackoffMaxMs);
}

void AppController::handleStatus(const Result& result) {
    const StatusInfo& info = result.info;
    bool changed = false;

    if (result.seq > playbackChangeSeq_) {
        if (state_.transport != info.transport || state_.currentUri != info.media.uri ||
            state_.currentTitle != info.media.title) {
            LOGI("Speaker: %s \"%s\"", sonos::transportStateName(info.transport),
                 info.media.title.c_str());
            state_.transport = info.transport;
            state_.currentUri = info.media.uri;
            state_.currentTitle = info.media.title;
            changed = true;
        }
    }

    if (result.seq > volumeChangeSeq_ && pendingVolume_ < 0) {
        int volume = info.volume;
        if (volume > config_.maxVolume) {
            LOGW("Volume %d above limit, reducing to %d", volume, config_.maxVolume);
            volume = config_.maxVolume;
            pendingVolume_ = volume;
        }
        if (!state_.volumeKnown || state_.volume != volume) {
            state_.volume = volume;
            state_.volumeKnown = true;
            changed = true;
        }
    }

    if (changed) {
        updateActiveFavorite();
        markChanged();
    }
}

void AppController::handleFavorites(const Result& result) {
    bool same = result.favorites.size() == state_.favorites.size();
    for (std::size_t i = 0; same && i < result.favorites.size(); ++i) {
        same = result.favorites[i].title == state_.favorites[i].title &&
               result.favorites[i].uri == state_.favorites[i].uri &&
               result.favorites[i].albumArtUri == state_.favorites[i].albumArtUri;
    }
    if (same) return;

    LOGI("%zu radio favorite(s) loaded", result.favorites.size());
    for (const sonos::Favorite& fav : result.favorites) LOGI("  %s", fav.title.c_str());
    state_.favorites = result.favorites;

    // Keep the logos of stations that are still there
    std::map<std::string, LogoPtr> logos;
    for (const sonos::Favorite& fav : state_.favorites) {
        auto it = state_.logos.find(fav.uri);
        if (it != state_.logos.end()) logos.insert(*it);
    }
    state_.logos = std::move(logos);
    updateActiveFavorite();
    markChanged();
}

void AppController::handleLogo(const Result& result, uint64_t now) {
    if (result.networkFailed()) {
        LOGD("Logo for %s: no response, retrying later", result.uri.c_str());
        nextLogoMs_ = now + config_.logoRetryMs;
        return;
    }
    const auto fav = std::find_if(state_.favorites.begin(), state_.favorites.end(),
                                  [&](const sonos::Favorite& f) { return f.uri == result.uri; });
    if (fav == state_.favorites.end()) return;
    LOGI("Logo \"%s\": %s (%u ms)", fav->title.c_str(), result.logo ? "loaded" : "none",
         static_cast<unsigned>(now - logoRequestedMs_));
    state_.logos[result.uri] = result.logo;
    if (result.logo) markChanged();
}

// Logos are fetched one at a time and only while the worker is idle, so a tap on a station
// or the volume never waits behind several image downloads.
void AppController::loadNextLogo(uint64_t now) {
    if (config_.logoSizePx <= 0 || commandsInFlight_ > 0 || pendingVolume_ >= 0 ||
        now < nextLogoMs_) {
        return;
    }
    for (const sonos::Favorite& fav : state_.favorites) {
        if (state_.logos.count(fav.uri) != 0) continue;
        Command cmd;
        cmd.type = CommandType::LoadLogo;
        cmd.uri = fav.uri;
        submit(cmd);
        logoRequestedMs_ = now;
        return;
    }
}

void AppController::trackFailures(const Result& result, uint64_t now) {
    if (!result.networkFailed()) {
        consecutiveFailures_ = 0;
        return;
    }
    ++consecutiveFailures_;
    LOGW("%s: speaker not responding (%d in a row)", commandName(result.type), consecutiveFailures_);
    if (consecutiveFailures_ >= config_.failuresUntilUnreachable &&
        state_.connection == Connection::Connected) {
        enterUnreachable(now);
    }
}

void AppController::enterUnreachable(uint64_t now) {
    LOGW("Speaker unreachable, searching again");
    state_.connection = Connection::Unreachable;
    state_.transport = sonos::TransportState::Unknown;
    pendingVolume_ = -1;
    backoffMs_ = config_.reconnectBackoffMinMs;
    nextDiscoveryMs_ = now + backoffMs_;
    backoffMs_ = std::min(backoffMs_ * 2, config_.reconnectBackoffMaxMs);
    updateActiveFavorite();
    markChanged();
}

void AppController::playFavorite(std::size_t index) {
    if (state_.connection != Connection::Connected || index >= state_.favorites.size()) return;
    const sonos::Favorite& fav = state_.favorites[index];
    LOGI("Play \"%s\"", fav.title.c_str());

    Command cmd;
    cmd.type = CommandType::PlayFavorite;
    cmd.uri = fav.uri;
    cmd.metadata = fav.metadata;
    const uint32_t seq = submit(cmd);
    if (seq == 0) return;
    playbackChangeSeq_ = seq;

    // Optimistic update until the speaker confirms
    state_.transport = sonos::TransportState::Transitioning;
    state_.currentUri = fav.uri;
    state_.currentTitle = fav.title;
    updateActiveFavorite();
    markChanged();
}

void AppController::stop() {
    if (state_.connection != Connection::Connected) return;
    LOGI("Stop");
    Command cmd;
    cmd.type = CommandType::Stop;
    const uint32_t seq = submit(cmd);
    if (seq == 0) return;
    playbackChangeSeq_ = seq;
    state_.transport = sonos::TransportState::Stopped;
    markChanged();
}

void AppController::changeVolume(int delta) {
    if (state_.connection != Connection::Connected || !state_.volumeKnown) return;
    const int volume = std::clamp(state_.volume + delta, 0, config_.maxVolume);
    if (volume == state_.volume) return;
    LOGD("Volume %d -> %d", state_.volume, volume);
    state_.volume = volume;
    pendingVolume_ = volume;
    markChanged();
    flushVolume(clock_.nowMs());
}

void AppController::flushVolume(uint64_t now) {
    if (pendingVolume_ < 0) return;
    if (volumeSent_ && now - lastVolumeSendMs_ < config_.volumeDebounceMs) return;

    Command cmd;
    cmd.type = CommandType::SetVolume;
    cmd.volume = pendingVolume_;
    const uint32_t seq = submit(cmd);
    if (seq == 0) return;
    volumeChangeSeq_ = seq;
    pendingVolume_ = -1;
    volumeSent_ = true;
    lastVolumeSendMs_ = now;
}

void AppController::updateActiveFavorite() {
    int active = -1;
    if (state_.connection == Connection::Connected) {
        for (std::size_t i = 0; i < state_.favorites.size(); ++i) {
            if (sonos::isSameStream(state_.favorites[i].uri, state_.currentUri)) {
                active = static_cast<int>(i);
                break;
            }
        }
        // Some services rewrite the stream URI; fall back to the station name.
        for (std::size_t i = 0; active < 0 && i < state_.favorites.size(); ++i) {
            if (!state_.currentTitle.empty() && state_.favorites[i].title == state_.currentTitle) {
                active = static_cast<int>(i);
            }
        }
    }
    state_.activeFavorite = active;
}

void AppController::noteUserActivity() {
    lastActivityMs_ = clock_.nowMs();
    setScreenPower(ScreenPower::On);
}

void AppController::updateDimming(uint64_t now) {
    const uint64_t idle = now - lastActivityMs_;
    if (idle >= config_.screenOffAfterMs) {
        setScreenPower(ScreenPower::Off);
    } else if (idle >= config_.dimAfterMs && screenPower_ == ScreenPower::On) {
        setScreenPower(ScreenPower::Dimmed);
    }
}

void AppController::setScreenPower(ScreenPower power) {
    if (power == screenPower_) return;
    const bool offChanged = (power == ScreenPower::Off) != (screenPower_ == ScreenPower::Off);
    screenPower_ = power;
    switch (power) {
    case ScreenPower::On: backlight_.setLevel(config_.backlightBright); break;
    case ScreenPower::Dimmed: backlight_.setLevel(config_.backlightDim); break;
    case ScreenPower::Off: backlight_.setLevel(0); break;
    }
    if (offChanged) markChanged();  // the UI shows or hides its wake-up overlay
}

} // namespace app
