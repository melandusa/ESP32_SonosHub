#pragma once

#include "app/LogoImage.h"

#include <string>

namespace ui {

// PNG -> square ARGB8888 logo: scaled to fit ("contain"), centered on a transparent background.
// Uses LVGL's lodepng, but no LVGL objects, so it may run in the worker context.
bool decodeLogo(const std::string& encoded, int sizePx, app::LogoImage& out);

} // namespace ui
