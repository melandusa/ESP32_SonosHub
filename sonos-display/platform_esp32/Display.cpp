#include "Display.h"

#include "Board.h"
#include "Gt911Touch.h"
#include "platform/Log.h"

#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>

namespace esp_platform {

namespace {

// LVGL renders into a partial buffer in internal RAM; GFX copies it to the PSRAM framebuffer.
constexpr int kBufferLines = 48;
// A calm tap easily lasts 0.5 s; LVGL's default of 400 ms would turn it into a long press,
// which repeats the volume buttons.
constexpr uint16_t kLongPressMs = 800;

Arduino_RGB_Display* gGfx = nullptr;
Gt911Touch gTouch;

void flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels) {
    const int32_t width = lv_area_get_width(area);
    const int32_t height = lv_area_get_height(area);
    gGfx->draw16bitRGBBitmap(static_cast<int16_t>(area->x1), static_cast<int16_t>(area->y1),
                             reinterpret_cast<uint16_t*>(pixels), static_cast<int16_t>(width),
                             static_cast<int16_t>(height));
    lv_display_flush_ready(display);
}

void readTouch(lv_indev_t* /*indev*/, lv_indev_data_t* data) {
    int16_t x = 0;
    int16_t y = 0;
    const bool pressed = gTouch.read(x, y);
    // The touch panel reports native panel coordinates; undo Arduino_GFX's rotation mapping.
    constexpr int16_t kMax = board::kScreenSize - 1;
    switch (board::kRotation) {
    case 1: data->point.x = y; data->point.y = kMax - x; break;
    case 2: data->point.x = kMax - x; data->point.y = kMax - y; break;
    case 3: data->point.x = kMax - y; data->point.y = x; break;
    default: data->point.x = x; data->point.y = y; break;
    }
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

} // namespace

bool isTouchPressed() {
    int16_t x = 0;
    int16_t y = 0;
    return gTouch.read(x, y);
}

lv_display_t* initDisplayAndTouch() {
    auto* bus = new Arduino_SWSPI(GFX_NOT_DEFINED /* DC */, board::kLcdCs, board::kLcdSck,
                                  board::kLcdMosi, GFX_NOT_DEFINED /* MISO */);
    auto* panel = new Arduino_ESP32RGBPanel(
        board::kLcdDe, board::kLcdVsync, board::kLcdHsync, board::kLcdPclk,
        board::kLcdR[0], board::kLcdR[1], board::kLcdR[2], board::kLcdR[3], board::kLcdR[4],
        board::kLcdG[0], board::kLcdG[1], board::kLcdG[2], board::kLcdG[3], board::kLcdG[4], board::kLcdG[5],
        board::kLcdB[0], board::kLcdB[1], board::kLcdB[2], board::kLcdB[3], board::kLcdB[4],
        1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
        1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */,
        0 /* pclk_active_neg */, board::kLcdPclkHz, false /* useBigEndian */,
        0 /* de_idle_high */, 0 /* pclk_idle_high */, board::kLcdBounceBufferPx);
    gGfx = new Arduino_RGB_Display(board::kScreenSize, board::kScreenSize, panel, board::kRotation,
                                   true /* auto_flush */, bus, GFX_NOT_DEFINED /* RST */,
                                   st7701_type9_init_operations, sizeof(st7701_type9_init_operations));
    if (!gGfx->begin()) {
        LOGE("Display: panel start failed");
        return nullptr;
    }
    gGfx->fillScreen(RGB565_BLACK);

    const size_t bufferBytes = board::kScreenSize * kBufferLines * sizeof(uint16_t);
    void* buffer = heap_caps_malloc(bufferBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buffer) {
        LOGE("Display: no memory for draw buffer");
        return nullptr;
    }

    lv_display_t* display = lv_display_create(board::kScreenSize, board::kScreenSize);
    lv_display_set_buffers(display, buffer, nullptr, bufferBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);

    gTouch.begin(board::kTouchSda, board::kTouchScl);
    lv_indev_t* touch = lv_indev_create();
    lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touch, readTouch);
    lv_indev_set_long_press_time(touch, kLongPressMs);

    return display;
}

} // namespace esp_platform
