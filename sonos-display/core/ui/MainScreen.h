#pragma once

#include "app/AppController.h"
#include "lvgl.h"

#include <array>
#include <cstdint>

namespace ui {

// The single screen of the app (480x480): header, 3x2 station tiles, control bar.
// Renders AppState and forwards taps to the AppController. UI thread only.
class MainScreen {
public:
    explicit MainScreen(app::AppController& controller);

    void create(lv_obj_t* screen);
    // Redraws if the controller state changed since the last call.
    void refresh();

private:
    static constexpr int kTileCount = 6;

    void createHeader(lv_obj_t* screen);
    void createTiles(lv_obj_t* screen);
    void createControls(lv_obj_t* screen);

    void render();
    void renderHeader(const app::AppState& state);
    void renderTiles(const app::AppState& state);
    void renderControls(const app::AppState& state);

    // Paging: up to 6 tiles; with more favorites, 5 stations + 1 "next page" tile.
    int favoritesPerPage() const;
    int pageCount() const;
    int favoriteIndexForTile(int tile) const;  // -1: empty or navigation tile
    bool isNavigationTile(int tile) const;

    static void onTileClicked(lv_event_t* e);
    static void onVolumeDown(lv_event_t* e);
    static void onVolumeUp(lv_event_t* e);
    static void onStop(lv_event_t* e);

    app::AppController& controller_;
    uint32_t renderedVersion_ = UINT32_MAX;
    int page_ = 0;

    lv_obj_t* titleLabel_ = nullptr;
    lv_obj_t* statusLabel_ = nullptr;
    lv_obj_t* messageLabel_ = nullptr;
    std::array<lv_obj_t*, kTileCount> tiles_{};
    std::array<lv_obj_t*, kTileCount> tileLabels_{};
    lv_obj_t* volumeDownButton_ = nullptr;
    lv_obj_t* volumeUpButton_ = nullptr;
    lv_obj_t* stopButton_ = nullptr;
    lv_obj_t* volumeBar_ = nullptr;
    lv_obj_t* volumeLabel_ = nullptr;
};

} // namespace ui
