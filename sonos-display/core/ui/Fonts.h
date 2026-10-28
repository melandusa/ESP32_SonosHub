#pragma once

#include "lvgl.h"

namespace ui::fonts {

// Creates the UI fonts (Montserrat with Latin-1 incl. umlauts, rendered by Tiny TTF).
// LVGL symbols (LV_SYMBOL_*) come from the built-in Montserrat as fallback.
void init();

const lv_font_t* header();  // 28 px
const lv_font_t* tile();    // 24 px
const lv_font_t* small();   // 24 px
const lv_font_t* button();  // 40 px

} // namespace ui::fonts
