#include "ui/LogoDecoder.h"

#include "lvgl.h"
// LVGL compiles lodepng as C; its C++ wrappers do not exist and would clash inside extern "C".
#define LODEPNG_NO_COMPILE_CPP
#include "src/libs/lodepng/lodepng.h"

#include <algorithm>
#include <cstring>

namespace ui {

namespace {

constexpr unsigned kMaxSourcePixels = 1000 * 1000;  // 4 MB RGBA while decoding (PSRAM on the ESP32)

bool isPng(const std::string& data) {
    static const unsigned char kSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    return data.size() > 24 && std::memcmp(data.data(), kSignature, 8) == 0;
}

// Reads width/height from the IHDR chunk without decoding.
void pngSize(const std::string& data, unsigned& width, unsigned& height) {
    auto be32 = [&](size_t at) {
        return (unsigned(uint8_t(data[at])) << 24) | (unsigned(uint8_t(data[at + 1])) << 16) |
               (unsigned(uint8_t(data[at + 2])) << 8) | unsigned(uint8_t(data[at + 3]));
    };
    width = be32(16);
    height = be32(20);
}

} // namespace

bool decodeLogo(const std::string& encoded, int sizePx, app::LogoImage& out) {
    if (sizePx <= 0 || !isPng(encoded)) return false;
    unsigned srcW = 0;
    unsigned srcH = 0;
    pngSize(encoded, srcW, srcH);
    if (srcW == 0 || srcH == 0 || srcW * srcH > kMaxSourcePixels) return false;

    // LVGL's lodepng returns an lv_draw_buf_t (RGBA byte order) instead of a plain buffer
    lv_draw_buf_t* decoded = nullptr;
    if (lodepng_decode32(reinterpret_cast<unsigned char**>(&decoded), &srcW, &srcH,
                         reinterpret_cast<const unsigned char*>(encoded.data()), encoded.size()) != 0) {
        if (decoded) lv_draw_buf_destroy(decoded);
        return false;
    }
    const uint8_t* rgba = decoded->data;
    const size_t stride = decoded->header.stride;

    // Fit into the square, keep aspect ratio
    const int size = sizePx;
    const float scale = std::min(float(size) / float(srcW), float(size) / float(srcH));
    const int dstW = std::max(1, int(float(srcW) * scale + 0.5f));
    const int dstH = std::max(1, int(float(srcH) * scale + 0.5f));
    const int offX = (size - dstW) / 2;
    const int offY = (size - dstH) / 2;

    out.size = size;
    out.pixels.assign(size_t(size) * size * 4, 0);  // fully transparent

    for (int y = 0; y < dstH; ++y) {
        // Source rows covered by this output row (box filter; at least one row when upscaling)
        const unsigned sy0 = unsigned(float(y) / scale);
        const unsigned sy1 = std::max(sy0 + 1, std::min(srcH, unsigned(float(y + 1) / scale)));
        for (int x = 0; x < dstW; ++x) {
            const unsigned sx0 = unsigned(float(x) / scale);
            const unsigned sx1 = std::max(sx0 + 1, std::min(srcW, unsigned(float(x + 1) / scale)));
            // Average with alpha weighting so transparent pixels do not darken the edges
            uint32_t r = 0, g = 0, b = 0, a = 0, count = 0;
            for (unsigned sy = sy0; sy < sy1 && sy < srcH; ++sy) {
                const uint8_t* p = rgba + size_t(sy) * stride + size_t(sx0) * 4;
                for (unsigned sx = sx0; sx < sx1 && sx < srcW; ++sx, p += 4) {
                    r += uint32_t(p[0]) * p[3];
                    g += uint32_t(p[1]) * p[3];
                    b += uint32_t(p[2]) * p[3];
                    a += p[3];
                    ++count;
                }
            }
            uint8_t* d = &out.pixels[(size_t(y + offY) * size + size_t(x + offX)) * 4];
            if (a > 0) {
                d[0] = uint8_t(b / a);
                d[1] = uint8_t(g / a);
                d[2] = uint8_t(r / a);
            }
            d[3] = uint8_t(count ? a / count : 0);
        }
    }

    lv_draw_buf_destroy(decoded);
    return true;
}

} // namespace ui
