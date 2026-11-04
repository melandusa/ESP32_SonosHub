#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace app {

// Station logo, ready to draw: square, size x size pixels, LVGL ARGB8888 byte order (B, G, R, A).
struct LogoImage {
    int size = 0;
    std::vector<uint8_t> pixels;
};

using LogoPtr = std::shared_ptr<const LogoImage>;

// Turns an encoded image (PNG) into a LogoImage of sizePx x sizePx. Runs in the worker
// context, so it must not touch LVGL objects.
using LogoDecoder = std::function<bool(const std::string& encoded, int sizePx, LogoImage& out)>;

} // namespace app
