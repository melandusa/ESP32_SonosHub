#include "ui/Fonts.h"

#include <cstddef>

// Generated at build time from LVGL's Montserrat-Medium.ttf (see CMakeLists.txt)
extern "C" {
extern const unsigned char montserrat_medium_ttf[];
extern const size_t montserrat_medium_ttf_size;
}

namespace ui::fonts {

namespace {

lv_font_t* gHeader = nullptr;
lv_font_t* gTile = nullptr;
lv_font_t* gSmall = nullptr;
lv_font_t* gButton = nullptr;

using GlyphDscCallback = bool (*)(const lv_font_t*, lv_font_glyph_dsc_t*, uint32_t, uint32_t);
GlyphDscCallback gTtfGlyphDsc = nullptr;

// LV_SYMBOL_* live in the Unicode private use area, which the TTF does not cover.
// Skipping them here avoids a Tiny TTF error log per lookup; the fallback font draws them.
bool glyphDscWithoutSymbols(const lv_font_t* font, lv_font_glyph_dsc_t* dsc, uint32_t letter,
                            uint32_t next) {
    if (letter >= 0xF000 && letter <= 0xF8FF) return false;
    return gTtfGlyphDsc(font, dsc, letter, next);
}

lv_font_t* create(int32_t size, const lv_font_t* symbolFallback) {
    // Kerning off: Tiny TTF looks up kerning pairs uncached for every character, which made a
    // full redraw take over a second on the ESP32-S3.
    lv_font_t* font = lv_tiny_ttf_create_data_ex(montserrat_medium_ttf, montserrat_medium_ttf_size, size,
                                                 LV_FONT_KERNING_NONE, LV_TINY_TTF_CACHE_GLYPH_CNT);
    if (!font) return nullptr;
    gTtfGlyphDsc = font->get_glyph_dsc;
    font->get_glyph_dsc = glyphDscWithoutSymbols;
    font->fallback = symbolFallback;
    return font;
}

const lv_font_t* orDefault(const lv_font_t* font, const lv_font_t* builtIn) {
    return font ? font : builtIn;
}

} // namespace

void init() {
    gHeader = create(28, &lv_font_montserrat_28);
    gTile = create(24, &lv_font_montserrat_24);
    gSmall = create(24, &lv_font_montserrat_24);
    gButton = create(40, &lv_font_montserrat_28);
    if (!gHeader || !gTile || !gSmall || !gButton) LV_LOG_ERROR("Tiny TTF font creation failed");
}

const lv_font_t* header() { return orDefault(gHeader, &lv_font_montserrat_28); }
const lv_font_t* tile() { return orDefault(gTile, &lv_font_montserrat_24); }
const lv_font_t* small() { return orDefault(gSmall, &lv_font_montserrat_24); }
const lv_font_t* button() { return orDefault(gButton, &lv_font_montserrat_28); }

} // namespace ui::fonts
