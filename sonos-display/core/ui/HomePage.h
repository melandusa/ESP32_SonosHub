#pragma once

#include "home/HomeController.h"
#include "lvgl.h"

#include <array>
#include <cstdint>

namespace ui {

// Homematic IP page below the header: 2x3 tiles. A socket tile toggles on tap (amber = on),
// a shutter tile has buttons up / stop / down. More than 6 channels: 5 + "next page" tile.
// UI thread only.
class HomePage {
public:
    explicit HomePage(home::HomeController& controller);

    void create(lv_obj_t* parent, int32_t top);
    void setVisible(bool visible);
    void render();

private:
    static constexpr int kTileCount = 6;

    struct Tile {
        lv_obj_t* box = nullptr;
        lv_obj_t* name = nullptr;
        lv_obj_t* state = nullptr;
        lv_obj_t* up = nullptr;
        lv_obj_t* stop = nullptr;
        lv_obj_t* down = nullptr;
    };

    int devicesPerPage() const;
    int pageCount() const;
    bool isNavigationTile(int tile) const;
    int deviceIndexForTile(int tile) const;  // -1: empty or navigation tile
    void renderTile(int slot, bool connected);

    static void onTileClicked(lv_event_t* e);
    static void onShutterButton(lv_event_t* e);

    home::HomeController& controller_;
    int page_ = 0;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* emptyLabel_ = nullptr;
    std::array<Tile, kTileCount> tiles_{};
};

} // namespace ui
