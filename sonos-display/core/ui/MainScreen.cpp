#include "ui/MainScreen.h"

#include "ui/Fonts.h"
#include "ui/Styles.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace ui {

namespace {

constexpr int32_t kScreenSize = 480;
constexpr int32_t kHeaderHeight = 70;
constexpr int32_t kGridTop = 76;
constexpr int32_t kGridHeight = 286;
constexpr int32_t kMargin = 8;
constexpr int32_t kGap = 8;
constexpr int32_t kControlsTop = kGridTop + kGridHeight + 8;
constexpr int32_t kTileWidth = (kScreenSize - 2 * kMargin - 2 * kGap) / 3;
constexpr int32_t kTileHeight = (kGridHeight - kGap) / 2;
constexpr int32_t kTilePadHor = 6;

const char* statusText(const app::AppState& state) {
    switch (state.transport) {
    case sonos::TransportState::Playing: return LV_SYMBOL_PLAY " läuft";
    case sonos::TransportState::Stopped: return LV_SYMBOL_STOP " gestoppt";
    case sonos::TransportState::Paused: return LV_SYMBOL_PAUSE " pausiert";
    case sonos::TransportState::Transitioning: return LV_SYMBOL_REFRESH " lädt…";
    case sonos::TransportState::Unknown: break;
    }
    return "";
}

lv_obj_t* createLabel(lv_obj_t* parent, const lv_font_t* font, lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

// Setting a label's text re-measures it even if nothing changed; text layout is the
// expensive part of a redraw on the ESP32.
bool setTextIfChanged(lv_obj_t* label, const char* text) {
    if (std::strcmp(lv_label_get_text(label), text) == 0) return false;
    lv_label_set_text(label, text);
    return true;
}

// Style setters invalidate the object even when the value stays the same.
void setBgColor(lv_obj_t* obj, lv_color_t color) {
    if (!lv_color_eq(lv_obj_get_style_bg_color(obj, LV_PART_MAIN), color)) {
        lv_obj_set_style_bg_color(obj, color, 0);
    }
}

void setTextColor(lv_obj_t* obj, lv_color_t color) {
    if (!lv_color_eq(lv_obj_get_style_text_color(obj, LV_PART_MAIN), color)) {
        lv_obj_set_style_text_color(obj, color, 0);
    }
}

// Wraps the text over up to three lines, centered; longer text ends with "...".
void setTileText(lv_obj_t* label, const char* text) {
    if (std::strcmp(lv_label_get_text(label), text) == 0) return;

    constexpr int32_t kMaxLines = 3;
    const lv_font_t* font = lv_obj_get_style_text_font(label, 0);
    const int32_t lineSpace = lv_obj_get_style_text_line_space(label, 0);
    const int32_t maxHeight = kMaxLines * lv_font_get_line_height(font) + (kMaxLines - 1) * lineSpace;
    lv_point_t size;
    lv_text_get_size(&size, text, font, 0, lineSpace, kTileWidth - 2 * kTilePadHor, LV_TEXT_FLAG_NONE);

    lv_label_set_text(label, text);
    if (size.y > maxHeight) {
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_set_height(label, maxHeight);
    } else {
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_height(label, LV_SIZE_CONTENT);
    }
    lv_obj_center(label);
}

// Station name below a logo: one line, "..." if too long.
void setTileCaption(lv_obj_t* label, const char* text) {
    if (std::strcmp(lv_label_get_text(label), text) == 0) return;
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_height(label, lv_font_get_line_height(lv_obj_get_style_text_font(label, 0)));
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, 0);
}

} // namespace

MainScreen::MainScreen(app::AppController& controller) : controller_(controller) {}

void MainScreen::create(lv_obj_t* screen) {
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, styles::background(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    createHeader(screen);
    createTiles(screen);
    createControls(screen);

    versionLabel_ = createLabel(screen, fonts::small(), styles::textDim());
    lv_obj_align(versionLabel_, LV_ALIGN_TOP_RIGHT, -kMargin - 6, kGridTop + kGridHeight - 34);
    lv_obj_add_flag(versionLabel_, LV_OBJ_FLAG_HIDDEN);

    createWakeOverlay(screen);
    render();
}

void MainScreen::createWakeOverlay(lv_obj_t* screen) {
    // Invisible, on top of everything while the screen saver is active: the first touch only
    // wakes the display and never reaches a tile or button.
    wakeOverlay_ = lv_obj_create(screen);
    lv_obj_remove_style_all(wakeOverlay_);
    lv_obj_set_size(wakeOverlay_, kScreenSize, kScreenSize);
    lv_obj_add_flag(wakeOverlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(wakeOverlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(wakeOverlay_, onWakePressed, LV_EVENT_PRESSED, this);
}

void MainScreen::createHeader(lv_obj_t* screen) {
    lv_obj_t* header = lv_obj_create(screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, kScreenSize, kHeaderHeight);
    lv_obj_set_style_bg_color(header, styles::headerBackground(), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    titleLabel_ = createLabel(header, fonts::header(), styles::text());
    lv_obj_set_width(titleLabel_, 290);
    lv_label_set_long_mode(titleLabel_, LV_LABEL_LONG_DOT);
    lv_obj_align(titleLabel_, LV_ALIGN_LEFT_MID, 14, 0);

    statusLabel_ = createLabel(header, fonts::small(), styles::textDim());
    lv_obj_set_width(statusLabel_, 160);
    lv_obj_set_style_text_align(statusLabel_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(statusLabel_, LV_ALIGN_RIGHT_MID, -14, 0);

    // Connection problems use the full header width
    messageLabel_ = createLabel(header, fonts::small(), styles::warning());
    lv_obj_set_width(messageLabel_, kScreenSize - 28);
    lv_label_set_long_mode(messageLabel_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(messageLabel_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(messageLabel_);
}

void MainScreen::createTiles(lv_obj_t* screen) {
    const int32_t width = kTileWidth;
    const int32_t height = kTileHeight;

    for (int i = 0; i < kTileCount; ++i) {
        lv_obj_t* tile = lv_button_create(screen);
        styles::applyButtonStyle(tile, styles::tile());
        lv_obj_set_style_pad_hor(tile, kTilePadHor, 0);
        lv_obj_set_size(tile, width, height);
        lv_obj_set_pos(tile, kMargin + (i % 3) * (width + kGap), kGridTop + (i / 3) * (height + kGap));
        lv_obj_add_event_cb(tile, onTileClicked, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(tile, reinterpret_cast<void*>(static_cast<intptr_t>(i)));

        lv_obj_t* image = lv_image_create(tile);
        lv_obj_align(image, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t* label = createLabel(tile, fonts::tile(), styles::text());
        lv_obj_set_width(label, lv_pct(100));
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(label);

        tiles_[i] = tile;
        tileLabels_[i] = label;
        tileImages_[i] = image;
    }
}

void MainScreen::createControls(lv_obj_t* screen) {
    const int32_t width = 140;
    const int32_t height = 64;
    const int32_t spacing = (kScreenSize - 3 * width) / 4;

    auto makeButton = [&](int column, const char* text, lv_color_t color, lv_event_cb_t callback,
                          bool repeat) {
        lv_obj_t* button = lv_button_create(screen);
        styles::applyButtonStyle(button, color);
        lv_obj_set_size(button, width, height);
        lv_obj_set_pos(button, spacing + column * (width + spacing), kControlsTop);
        if (repeat) {
            // React on touch-down and keep stepping while held
            lv_obj_add_event_cb(button, callback, LV_EVENT_PRESSED, this);
            lv_obj_add_event_cb(button, callback, LV_EVENT_LONG_PRESSED_REPEAT, this);
        } else {
            lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, this);
        }
        lv_obj_t* label = createLabel(button, fonts::button(), styles::text());
        lv_label_set_text(label, text);
        lv_obj_center(label);
        return button;
    };
    volumeDownButton_ = makeButton(0, "−", styles::tile(), onVolumeDown, true);
    stopButton_ = makeButton(1, "AUS", styles::stopButton(), onStop, false);
    volumeUpButton_ = makeButton(2, "+", styles::tile(), onVolumeUp, true);

    // Volume bar with numeric value
    const int32_t barTop = kControlsTop + height + 14;
    volumeBar_ = lv_bar_create(screen);
    lv_obj_remove_style_all(volumeBar_);
    lv_obj_set_size(volumeBar_, 250, 22);
    lv_obj_set_pos(volumeBar_, spacing, barTop + 2);
    lv_obj_set_style_radius(volumeBar_, 11, 0);
    lv_obj_set_style_bg_opa(volumeBar_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(volumeBar_, styles::volumeTrack(), 0);
    lv_obj_set_style_radius(volumeBar_, 11, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(volumeBar_, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volumeBar_, styles::volumeFill(), LV_PART_INDICATOR);
    lv_bar_set_range(volumeBar_, 0, controller_.config().maxVolume);

    volumeLabel_ = createLabel(screen, fonts::small(), styles::text());
    lv_obj_set_pos(volumeLabel_, spacing + 250 + 14, barTop - 2);
}

int MainScreen::favoritesPerPage() const {
    const auto count = controller_.state().favorites.size();
    return count <= static_cast<std::size_t>(kTileCount) ? kTileCount : kTileCount - 1;
}

int MainScreen::pageCount() const {
    const int count = static_cast<int>(controller_.state().favorites.size());
    const int perPage = favoritesPerPage();
    return count == 0 ? 1 : (count + perPage - 1) / perPage;
}

bool MainScreen::isNavigationTile(int tile) const {
    return pageCount() > 1 && tile == kTileCount - 1;
}

int MainScreen::favoriteIndexForTile(int tile) const {
    if (isNavigationTile(tile)) return -1;
    const int index = page_ * favoritesPerPage() + tile;
    return index < static_cast<int>(controller_.state().favorites.size()) ? index : -1;
}

void MainScreen::refresh() {
    if (controller_.version() != renderedVersion_) render();
}

void MainScreen::setSystemMessage(const std::string& message) {
    if (message == systemMessage_) return;
    systemMessage_ = message;
    render();
}

void MainScreen::setVersionText(const std::string& text) {
    if (text == versionText_) return;
    versionText_ = text;
    render();
}

void MainScreen::render() {
    const app::AppState& state = controller_.state();
    renderedVersion_ = controller_.version();
    if (page_ >= pageCount()) page_ = 0;

    renderHeader(state);
    renderTiles(state);
    renderControls(state);
    if (!versionText_.empty() && state.favorites.empty()) {
        setTextIfChanged(versionLabel_, versionText_.c_str());
        lv_obj_remove_flag(versionLabel_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(versionLabel_, LV_OBJ_FLAG_HIDDEN);
    }
    if (controller_.isScreenOff()) {
        lv_obj_remove_flag(wakeOverlay_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(wakeOverlay_, LV_OBJ_FLAG_HIDDEN);
    }
}

void MainScreen::renderHeader(const app::AppState& state) {
    const bool connected = state.connection == app::Connection::Connected;
    const char* message = nullptr;
    if (!systemMessage_.empty()) {
        message = systemMessage_.c_str();
    } else if (state.connection == app::Connection::Connecting) {
        message = LV_SYMBOL_REFRESH " verbindet…";
    } else if (state.connection == app::Connection::Unreachable) {
        message = LV_SYMBOL_WARNING " Lautsprecher nicht erreichbar – suche…";
    }

    if (message) {
        setTextIfChanged(messageLabel_, message);
        lv_obj_remove_flag(messageLabel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(titleLabel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(statusLabel_, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_add_flag(messageLabel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(titleLabel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(statusLabel_, LV_OBJ_FLAG_HIDDEN);

    // Station while something is selected, otherwise the room name
    const bool hasStation = connected && !state.currentTitle.empty() &&
                            state.transport != sonos::TransportState::Stopped;
    const std::string title = hasStation ? std::string(LV_SYMBOL_AUDIO " ") + state.currentTitle
                                         : state.roomName;
    setTextIfChanged(titleLabel_, title.c_str());
    setTextIfChanged(statusLabel_, statusText(state));
}

void MainScreen::renderTiles(const app::AppState& state) {
    const bool connected = state.connection == app::Connection::Connected;
    const bool playing = state.transport == sonos::TransportState::Playing ||
                         state.transport == sonos::TransportState::Transitioning;

    for (int i = 0; i < kTileCount; ++i) {
        lv_obj_t* tile = tiles_[i];
        lv_obj_t* label = tileLabels_[i];

        if (isNavigationTile(i)) {
            const std::string text = std::string(LV_SYMBOL_RIGHT "\nSeite ") +
                                     std::to_string(page_ + 1) + "/" + std::to_string(pageCount());
            setTileLogo(i, nullptr);
            setTileText(label, text.c_str());
            setBgColor(tile, styles::headerBackground());
            setTextColor(label, styles::textDim());
            lv_obj_remove_flag(tile, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_state(tile, LV_STATE_DISABLED);
            continue;
        }

        const int index = favoriteIndexForTile(i);
        if (index < 0) {
            lv_obj_add_flag(tile, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        const bool active = playing && index == state.activeFavorite;
        const sonos::Favorite& fav = state.favorites[static_cast<std::size_t>(index)];
        const auto logo = state.logos.find(fav.uri);
        setTileLogo(i, logo != state.logos.end() ? logo->second : nullptr);
        if (tileLogos_[i]) {
            setTileCaption(label, fav.title.c_str());
        } else {
            setTileText(label, fav.title.c_str());
        }
        setBgColor(tile, active ? styles::active() : styles::tile());
        setTextColor(label, active ? styles::activeText() : styles::text());
        lv_obj_remove_flag(tile, LV_OBJ_FLAG_HIDDEN);
        if (connected) {
            lv_obj_remove_state(tile, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(tile, LV_STATE_DISABLED);
        }
    }
}

void MainScreen::setTileLogo(int tile, const app::LogoPtr& logo) {
    if (logo == tileLogos_[tile]) return;
    lv_obj_t* image = tileImages_[tile];
    lv_image_dsc_t& dsc = tileImageDscs_[tile];
    if (tileLogos_[tile]) lv_image_cache_drop(&dsc);
    tileLogos_[tile] = logo;
    // Text-only and logo tiles lay out the label differently; force the next set*Text to redo it.
    lv_label_set_text(tileLabels_[tile], "");

    if (!logo) {
        lv_obj_add_flag(image, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    dsc = {};
    dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    dsc.header.w = static_cast<uint16_t>(logo->size);
    dsc.header.h = static_cast<uint16_t>(logo->size);
    dsc.header.stride = static_cast<uint16_t>(logo->size * 4);
    dsc.data = logo->pixels.data();
    dsc.data_size = static_cast<uint32_t>(logo->pixels.size());
    lv_image_set_src(image, &dsc);
    lv_obj_remove_flag(image, LV_OBJ_FLAG_HIDDEN);
}

void MainScreen::renderControls(const app::AppState& state) {
    const bool connected = state.connection == app::Connection::Connected;
    for (lv_obj_t* button : {volumeDownButton_, volumeUpButton_, stopButton_}) {
        if (connected) {
            lv_obj_remove_state(button, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(button, LV_STATE_DISABLED);
        }
    }

    if (connected && state.volumeKnown) {
        lv_bar_set_value(volumeBar_, state.volume, LV_ANIM_OFF);
        char text[24];
        std::snprintf(text, sizeof(text), "Lautstärke %d", state.volume);
        setTextIfChanged(volumeLabel_, text);
    } else {
        lv_bar_set_value(volumeBar_, 0, LV_ANIM_OFF);
        setTextIfChanged(volumeLabel_, "Lautstärke –");
    }
}

void MainScreen::onTileClicked(lv_event_t* e) {
    auto* self = static_cast<MainScreen*>(lv_event_get_user_data(e));
    auto* tile = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    const int slot = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(tile)));

    if (self->isNavigationTile(slot)) {
        self->page_ = (self->page_ + 1) % self->pageCount();
        self->render();
        return;
    }
    const int index = self->favoriteIndexForTile(slot);
    if (index >= 0) self->controller_.playFavorite(static_cast<std::size_t>(index));
}

void MainScreen::onVolumeDown(lv_event_t* e) {
    static_cast<MainScreen*>(lv_event_get_user_data(e))->controller_.volumeDown();
}

void MainScreen::onVolumeUp(lv_event_t* e) {
    static_cast<MainScreen*>(lv_event_get_user_data(e))->controller_.volumeUp();
}

void MainScreen::onStop(lv_event_t* e) {
    static_cast<MainScreen*>(lv_event_get_user_data(e))->controller_.stop();
}

void MainScreen::onWakePressed(lv_event_t* e) {
    static_cast<MainScreen*>(lv_event_get_user_data(e))->controller_.noteUserActivity();
}

} // namespace ui
