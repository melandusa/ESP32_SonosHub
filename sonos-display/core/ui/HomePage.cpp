#include "ui/HomePage.h"

#include "ui/Fonts.h"
#include "ui/Styles.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace ui {

namespace {

constexpr int32_t kScreenSize = 480;
constexpr int32_t kMargin = 8;
constexpr int32_t kGap = 8;
constexpr int32_t kTilePad = 8;
constexpr int32_t kTileWidth = (kScreenSize - 2 * kMargin - kGap) / 2;
constexpr int32_t kButtonHeight = 56;

enum ShutterAction : intptr_t { kUp = 0, kStop = 1, kDown = 2 };

bool setTextIfChanged(lv_obj_t* label, const char* text) {
    if (std::strcmp(lv_label_get_text(label), text) == 0) return false;
    lv_label_set_text(label, text);
    return true;
}

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

void setHidden(lv_obj_t* obj, bool hidden) {
    if (hidden == lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    if (hidden) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

void setEnabled(lv_obj_t* obj, bool enabled) {
    if (enabled) {
        lv_obj_remove_state(obj, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(obj, LV_STATE_DISABLED);
    }
}

lv_obj_t* createLabel(lv_obj_t* parent, const lv_font_t* font, lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_text(label, "");
    return label;
}

std::string shutterText(const home::Device& device) {
    if (!device.reachable) return "nicht erreichbar";
    if (device.moving) return "fährt…";
    if (device.level < 0) return "–";
    if (device.level <= 0.02) return "offen";
    if (device.level >= 0.98) return "zu";
    return std::to_string(static_cast<int>(std::lround(device.level * 100))) + " % zu";
}

} // namespace

HomePage::HomePage(home::HomeController& controller) : controller_(controller) {}

void HomePage::create(lv_obj_t* parent, int32_t top) {
    const int32_t height = kScreenSize - top - kMargin;
    const int32_t tileHeight = (height - 2 * kGap) / 3;

    root_ = lv_obj_create(parent);
    lv_obj_remove_style_all(root_);
    lv_obj_set_pos(root_, 0, top);
    lv_obj_set_size(root_, kScreenSize, height);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    emptyLabel_ = createLabel(root_, fonts::small(), styles::textDim());
    lv_obj_set_width(emptyLabel_, kScreenSize - 4 * kMargin);
    lv_label_set_long_mode(emptyLabel_, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(emptyLabel_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(emptyLabel_, "Keine Steckdosen oder Rollläden gefunden");
    lv_obj_center(emptyLabel_);
    lv_obj_add_flag(emptyLabel_, LV_OBJ_FLAG_HIDDEN);

    const int32_t buttonWidth = (kTileWidth - 2 * kTilePad - 2 * 6) / 3;
    for (int i = 0; i < kTileCount; ++i) {
        Tile& tile = tiles_[i];
        tile.box = lv_button_create(root_);
        styles::applyButtonStyle(tile.box, styles::tile());
        lv_obj_set_style_pad_all(tile.box, kTilePad, 0);
        lv_obj_set_size(tile.box, kTileWidth, tileHeight);
        lv_obj_set_pos(tile.box, kMargin + (i % 2) * (kTileWidth + kGap), (i / 2) * (tileHeight + kGap));
        lv_obj_remove_flag(tile.box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_user_data(tile.box, reinterpret_cast<void*>(static_cast<intptr_t>(i)));
        lv_obj_add_event_cb(tile.box, onTileClicked, LV_EVENT_CLICKED, this);

        tile.name = createLabel(tile.box, fonts::tile(), styles::text());
        lv_obj_set_width(tile.name, lv_pct(100));
        lv_label_set_long_mode(tile.name, LV_LABEL_LONG_DOT);
        lv_obj_align(tile.name, LV_ALIGN_TOP_LEFT, 0, 0);

        tile.state = createLabel(tile.box, fonts::small(), styles::textDim());
        lv_obj_set_width(tile.state, lv_pct(100));
        lv_label_set_long_mode(tile.state, LV_LABEL_LONG_DOT);
        lv_obj_align(tile.state, LV_ALIGN_TOP_LEFT, 0, 30);

        auto makeButton = [&](int column, const char* symbol, intptr_t action) {
            lv_obj_t* button = lv_button_create(tile.box);
            styles::applyButtonStyle(button, action == kStop ? styles::headerBackground()
                                                             : styles::tilePressed());
            lv_obj_set_size(button, buttonWidth, kButtonHeight);
            lv_obj_align(button, LV_ALIGN_BOTTOM_LEFT, column * (buttonWidth + 6), 0);
            lv_obj_set_user_data(button, reinterpret_cast<void*>(i * 4 + action));
            lv_obj_add_event_cb(button, onShutterButton, LV_EVENT_CLICKED, this);
            lv_obj_t* label = createLabel(button, fonts::header(), styles::text());
            lv_label_set_text(label, symbol);
            lv_obj_center(label);
            return button;
        };
        tile.up = makeButton(0, LV_SYMBOL_UP, kUp);
        tile.stop = makeButton(1, LV_SYMBOL_STOP, kStop);
        tile.down = makeButton(2, LV_SYMBOL_DOWN, kDown);
    }
    render();
}

void HomePage::setVisible(bool visible) { setHidden(root_, !visible); }

int HomePage::devicesPerPage() const {
    const auto count = controller_.state().devices.size();
    return count <= static_cast<std::size_t>(kTileCount) ? kTileCount : kTileCount - 1;
}

int HomePage::pageCount() const {
    const int count = static_cast<int>(controller_.state().devices.size());
    const int perPage = devicesPerPage();
    return count == 0 ? 1 : (count + perPage - 1) / perPage;
}

bool HomePage::isNavigationTile(int tile) const { return pageCount() > 1 && tile == kTileCount - 1; }

int HomePage::deviceIndexForTile(int tile) const {
    if (isNavigationTile(tile)) return -1;
    const int index = page_ * devicesPerPage() + tile;
    return index < static_cast<int>(controller_.state().devices.size()) ? index : -1;
}

void HomePage::render() {
    const home::HomeState& state = controller_.state();
    if (page_ >= pageCount()) page_ = 0;
    const bool connected = state.connection == home::HomeConnection::Connected;
    setHidden(emptyLabel_, !(connected && state.devices.empty()));
    for (int i = 0; i < kTileCount; ++i) renderTile(i, connected);
}

void HomePage::renderTile(int slot, bool connected) {
    Tile& tile = tiles_[slot];
    if (isNavigationTile(slot)) {
        const std::string text = std::string(LV_SYMBOL_RIGHT " Seite ") + std::to_string(page_ + 1) + "/" +
                                 std::to_string(pageCount());
        setTextIfChanged(tile.name, text.c_str());
        setTextIfChanged(tile.state, "");
        for (lv_obj_t* button : {tile.up, tile.stop, tile.down}) setHidden(button, true);
        setBgColor(tile.box, styles::headerBackground());
        setTextColor(tile.name, styles::textDim());
        lv_obj_add_flag(tile.box, LV_OBJ_FLAG_CLICKABLE);
        setEnabled(tile.box, true);
        setHidden(tile.box, false);
        return;
    }

    const int index = deviceIndexForTile(slot);
    if (index < 0) {
        setHidden(tile.box, true);
        return;
    }
    const home::Device& device = controller_.state().devices[static_cast<std::size_t>(index)];
    const bool usable = connected && device.reachable;
    setTextIfChanged(tile.name, device.name.c_str());

    if (device.kind == home::DeviceKind::Switch) {
        const bool on = device.on && device.reachable;
        setTextIfChanged(tile.state, !device.reachable ? "nicht erreichbar" : on ? "AN" : "AUS");
        for (lv_obj_t* button : {tile.up, tile.stop, tile.down}) setHidden(button, true);
        setBgColor(tile.box, on ? styles::active() : styles::tile());
        setTextColor(tile.name, on ? styles::activeText() : styles::text());
        setTextColor(tile.state, on ? styles::activeText() : styles::textDim());
        lv_obj_add_flag(tile.box, LV_OBJ_FLAG_CLICKABLE);
    } else {
        setTextIfChanged(tile.state, shutterText(device).c_str());
        for (lv_obj_t* button : {tile.up, tile.stop, tile.down}) {
            setHidden(button, false);
            setEnabled(button, usable);
        }
        setBgColor(tile.box, styles::tile());
        setTextColor(tile.name, styles::text());
        setTextColor(tile.state, styles::textDim());
        lv_obj_remove_flag(tile.box, LV_OBJ_FLAG_CLICKABLE);  // only the buttons react
    }
    setEnabled(tile.box, usable);
    setHidden(tile.box, false);
}

void HomePage::onTileClicked(lv_event_t* e) {
    auto* self = static_cast<HomePage*>(lv_event_get_user_data(e));
    auto* box = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    const int slot = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(box)));
    if (self->isNavigationTile(slot)) {
        self->page_ = (self->page_ + 1) % self->pageCount();
        self->render();
        return;
    }
    const int index = self->deviceIndexForTile(slot);
    if (index >= 0) self->controller_.toggleSwitch(static_cast<std::size_t>(index));
}

void HomePage::onShutterButton(lv_event_t* e) {
    auto* self = static_cast<HomePage*>(lv_event_get_user_data(e));
    auto* button = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
    const auto data = reinterpret_cast<intptr_t>(lv_obj_get_user_data(button));
    const int index = self->deviceIndexForTile(static_cast<int>(data / 4));
    if (index < 0) return;
    const auto device = static_cast<std::size_t>(index);
    switch (data % 4) {
    case kUp: self->controller_.openShutter(device); break;
    case kStop: self->controller_.stopShutter(device); break;
    case kDown: self->controller_.closeShutter(device); break;
    default: break;
    }
}

} // namespace ui
