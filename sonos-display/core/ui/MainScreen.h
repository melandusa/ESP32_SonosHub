#pragma once

#include "app/AppController.h"
#include "home/HomeController.h"
#include "lvgl.h"
#include "ui/HomePage.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace ui {

// The screen of the app (480x480): header, 3x2 station tiles, control bar.
// Renders AppState and forwards taps to the AppController. With Homematic IP enabled, a
// button in the header switches to the HomePage (sockets, shutters). UI thread only.
class MainScreen {
public:
    explicit MainScreen(app::AppController& controller);

    void create(lv_obj_t* screen);
    // Adds the Homematic IP page and the page button in the header. Call once after create().
    void enableHome(home::HomeController& home);
    // Redraws if the controller state changed since the last call.
    void refresh();
    // Platform message shown in the header instead of the speaker status (e.g. WLAN setup);
    // empty to clear.
    void setSystemMessage(const std::string& message);
    // Shown in the empty station area until the favorites are loaded (e.g. "Version 3").
    void setVersionText(const std::string& text);

private:
    static constexpr int kTileCount = 6;
    enum class Page { Music, Home };

    void createHeader(lv_obj_t* screen);
    void createTiles(lv_obj_t* screen);
    void createControls(lv_obj_t* screen);
    void createWakeOverlay(lv_obj_t* screen);

    void render();
    void renderHeader(const app::AppState& state);
    void renderHomeHeader();
    void showHeaderMessage(const char* message);
    void layoutHeader();
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
    static void onPageButton(lv_event_t* e);

    app::AppController& controller_;
    uint32_t renderedVersion_ = UINT32_MAX;
    home::HomeController* home_ = nullptr;
    std::unique_ptr<HomePage> homePage_;
    uint32_t renderedHomeVersion_ = UINT32_MAX;
    Page shownPage_ = Page::Music;
    int page_ = 0;
    std::string systemMessage_;
    std::string versionText_;

    lv_obj_t* musicPage_ = nullptr;
    lv_obj_t* pageButton_ = nullptr;
    lv_obj_t* pageButtonLabel_ = nullptr;
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
    lv_obj_t* versionLabel_ = nullptr;
    lv_obj_t* wakeOverlay_ = nullptr;
};

} // namespace ui
