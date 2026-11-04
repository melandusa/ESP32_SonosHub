#pragma once

#include "app/AppController.h"
#include "lvgl.h"

#include <array>
#include <cstdint>
#include <string>

namespace ui {

// The single screen of the app (480x480): header, 3x2 station tiles, control bar.
// Renders AppState and forwards taps to the AppController. UI thread only.
class MainScreen {
public:
    explicit MainScreen(app::AppController& controller);

    void create(lv_obj_t* screen);
    // Redraws if the controller state changed since the last call.
    void refresh();
    // Platform message shown in the header instead of the speaker status (e.g. WLAN setup);
    // empty to clear.
    void setSystemMessage(const std::string& message);

private:
    static constexpr int kTileCount = 6;

    void createHeader(lv_obj_t* screen);
    void createTiles(lv_obj_t* screen);
    void createControls(lv_obj_t* screen);
    void createWakeOverlay(lv_obj_t* screen);

    void render();
    void renderHeader(const app::AppState& state);
    void renderTiles(const app::AppState& state);
    void renderControls(const app::AppState& state);
    void setTileLogo(int tile, const app::LogoPtr& logo);

    // Paging: up to 6 tiles; with more favorites, 5 stations + 1 "next page" tile.
    int favoritesPerPage() const;
    int pageCount() const;
    int favoriteIndexForTile(int tile) const;  // -1: empty or navigation tile
    bool isNavigationTile(int tile) const;

    static void onTileClicked(lv_event_t* e);
    static void onVolumeDown(lv_event_t* e);
    static void onVolumeUp(lv_event_t* e);
    static void onStop(lv_event_t* e);
    static void onWakePressed(lv_event_t* e);

    app::AppController& controller_;
    uint32_t renderedVersion_ = UINT32_MAX;
    int page_ = 0;
    std::string systemMessage_;

    lv_obj_t* titleLabel_ = nullptr;
    lv_obj_t* statusLabel_ = nullptr;
    lv_obj_t* messageLabel_ = nullptr;
    std::array<lv_obj_t*, kTileCount> tiles_{};
    std::array<lv_obj_t*, kTileCount> tileLabels_{};
    std::array<lv_obj_t*, kTileCount> tileImages_{};
    std::array<lv_image_dsc_t, kTileCount> tileImageDscs_{};
    std::array<app::LogoPtr, kTileCount> tileLogos_{};  // keeps the pixels alive while shown
    lv_obj_t* volumeDownButton_ = nullptr;
    lv_obj_t* volumeUpButton_ = nullptr;
    lv_obj_t* stopButton_ = nullptr;
    lv_obj_t* volumeBar_ = nullptr;
    lv_obj_t* volumeLabel_ = nullptr;
    lv_obj_t* wakeOverlay_ = nullptr;
};

} // namespace ui
