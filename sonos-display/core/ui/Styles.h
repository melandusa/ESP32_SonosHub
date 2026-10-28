#pragma once

#include "lvgl.h"

namespace ui::styles {

// High-contrast dark theme
inline lv_color_t background() { return lv_color_hex(0x0E1114); }
inline lv_color_t headerBackground() { return lv_color_hex(0x1A1F24); }
inline lv_color_t tile() { return lv_color_hex(0x2B323A); }
inline lv_color_t tilePressed() { return lv_color_hex(0x46505B); }
inline lv_color_t active() { return lv_color_hex(0xFFB300); }  // amber: currently playing
inline lv_color_t activeText() { return lv_color_hex(0x000000); }
inline lv_color_t text() { return lv_color_hex(0xFFFFFF); }
inline lv_color_t textDim() { return lv_color_hex(0xB0BAC4); }
inline lv_color_t stopButton() { return lv_color_hex(0xC62828); }
inline lv_color_t warning() { return lv_color_hex(0xFF7043); }
inline lv_color_t volumeTrack() { return lv_color_hex(0x2B323A); }
inline lv_color_t volumeFill() { return lv_color_hex(0x4FC3F7); }

// Common look for all tappable elements: rounded, pressed = darker and shifted down.
void applyButtonStyle(lv_obj_t* button, lv_color_t color);

} // namespace ui::styles
