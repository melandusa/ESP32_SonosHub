#pragma once

#include <cstddef>
#include <cstdint>

// Guition ESP32-S3-4848S040 (ESP32-S3, 16 MB flash, 8 MB OPI PSRAM).
// Display pins and timings as in the board definition of "GFX Library for Arduino".
namespace board {

constexpr int kScreenSize = 480;

// ST7701 init via 3-wire SPI
constexpr int8_t kLcdCs = 39;
constexpr int8_t kLcdSck = 48;
constexpr int8_t kLcdMosi = 47;

// RGB bus
constexpr int8_t kLcdDe = 18;
constexpr int8_t kLcdVsync = 17;
constexpr int8_t kLcdHsync = 16;
constexpr int8_t kLcdPclk = 21;
constexpr int8_t kLcdR[5] = {11, 12, 13, 14, 0};
constexpr int8_t kLcdG[6] = {8, 20, 3, 46, 9, 10};
constexpr int8_t kLcdB[5] = {4, 5, 6, 7, 15};
constexpr int32_t kLcdPclkHz = 12000000;
// The panel DMA reads the framebuffer from PSRAM. Bounce buffers in internal RAM keep it fed
// while WLAN and drawing also use PSRAM; without them the picture flickers.
constexpr size_t kLcdBounceBufferPx = kScreenSize * 10;

// Rotation of the picture in 90 degree steps (0..3), as in Arduino_GFX. 0 is the panel's
// native orientation; 3 fits the printed holder. Touch coordinates follow automatically.
constexpr uint8_t kRotation = 3;

constexpr int8_t kBacklight = 38;

// GT911 touch controller (INT and RST are not connected)
constexpr int8_t kTouchSda = 19;
constexpr int8_t kTouchScl = 45;

} // namespace board
