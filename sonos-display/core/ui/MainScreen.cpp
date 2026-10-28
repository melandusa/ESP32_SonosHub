#include "ui/MainScreen.h"

#include "ui/Fonts.h"
#include "ui/Styles.h"

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

// Wraps the text over up to three lines, centered; longer text ends with "...".
void setTileText(lv_obj_t* label, const char* text) {
    constexpr int32_t kMaxLines = 3;
    const lv_font_t* font = lv_obj_get_style_text_font(label, 0);
    const int32_t lineSpace = lv_obj_get_style_text_line_space(label, 0);
    const int32_t maxHeight = kMaxLines * lv_font_get_line_height(font) + (kMaxLines - 1) * lineSpace;

    lv_obj_t* tile = lv_obj_get_parent(label);
    lv_obj_update_layout(tile);
    const int32_t width = lv_obj_get_content_width(tile);
    lv_point_t size;
    lv_text_get_size(&size, text, font, 0, lineSpace, width, LV_TEXT_FLAG_NONE);

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
    render();
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
    const int32_t width = (kScreenSize - 2 * kMargin - 2 * kGap) / 3;
    const int32_t height = (kGridHeight - kGap) / 2;

    for (int i = 0; i < kTileCount; ++i) {
        lv_obj_t* tile = lv_button_create(screen);
        styles::applyButtonStyle(tile, styles::tile());
        lv_obj_set_style_pad_hor(tile, 6, 0);
        lv_obj_set_size(tile, width, height);
        lv_obj_set_pos(tile, kMargin + (i % 3) * (width + kGap), kGridTop + (i / 3) * (height + kGap));
        lv_obj_add_event_cb(tile, onTileClicked, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(tile, reinterpret_cast<void*>(static_cast<intptr_t>(i)));

        lv_obj_t* label = createLabel(tile, fonts::tile(), styles::text());
        lv_obj_set_width(label, lv_pct(100));
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(label);

        tiles_[i] = tile;
        tileLabels_[i] = label;
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

void MainScreen::render() {
    const app::AppState& state = controller_.state();
    renderedVersion_ = controller_.version();
    if (page_ >= pageCount()) page_ = 0;

    renderHeader(state);
    renderTiles(state);
    renderControls(state);
}

void MainScreen::renderHeader(const app::AppState& state) {
    const bool connected = state.connection == app::Connection::Connected;
    const char* message = nullptr;
    if (state.connection == app::Connection::Connecting) {
        message = LV_SYMBOL_REFRESH " verbindet…";
    } else if (state.connection == app::Connection::Unreachable) {
        message = LV_SYMBOL_WARNING " Lautsprecher nicht erreichbar – suche…";
    }

    if (message) {
        lv_label_set_text(messageLabel_, message);
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
    lv_label_set_text(titleLabel_, title.c_str());
    lv_label_set_text(statusLabel_, statusText(state));
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
            setTileText(label, text.c_str());
            lv_obj_set_style_bg_color(tile, styles::headerBackground(), 0);
            lv_obj_set_style_text_color(label, styles::textDim(), 0);
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
        setTileText(label, state.favorites[static_cast<std::size_t>(index)].title.c_str());
        lv_obj_set_style_bg_color(tile, active ? styles::active() : styles::tile(), 0);
        lv_obj_set_style_text_color(label, active ? styles::activeText() : styles::text(), 0);
        lv_obj_remove_flag(tile, LV_OBJ_FLAG_HIDDEN);
        if (connected) {
            lv_obj_remove_state(tile, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(tile, LV_STATE_DISABLED);
        }
    }
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
        lv_label_set_text_fmt(volumeLabel_, "Lautstärke %d", state.volume);
    } else {
        lv_bar_set_value(volumeBar_, 0, LV_ANIM_OFF);
        lv_label_set_text(volumeLabel_, "Lautstärke –");
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

} // namespace ui
