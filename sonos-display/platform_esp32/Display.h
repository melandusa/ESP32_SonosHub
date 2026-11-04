#pragma once

#include "lvgl.h"

namespace esp_platform {

// Brings up the ST7701 RGB panel and the GT911 touch and registers both with LVGL.
// Call after lv_init(). Returns nullptr if the panel could not be started.
lv_display_t* initDisplayAndTouch();

} // namespace esp_platform
