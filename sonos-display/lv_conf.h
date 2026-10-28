/**
 * LVGL configuration for the Sonos display.
 * Only settings that differ from LVGL's defaults are listed here;
 * everything else falls back to lv_conf_internal.h.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* Color depth: 32 for the SDL simulator; the ESP32 build will use 16 (RGB565) */
#define LV_COLOR_DEPTH 32

/* Memory: system malloc (the ESP32 build will route this to PSRAM) */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB

/* Bare-metal style: all LVGL calls happen in the UI thread */
#define LV_USE_OS LV_OS_NONE

#define LV_DEF_REFR_PERIOD 33

/* Logging */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1

/* Built-in fonts (ASCII + LV_SYMBOL_*); umlauts come from the Tiny TTF fonts in core/ui/Fonts.cpp */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT &lv_font_montserrat_24

/* Runtime TTF rendering for the UI fonts (Montserrat with umlauts, embedded as C array) */
#define LV_USE_TINY_TTF 1
#if LV_USE_TINY_TTF
    #define LV_TINY_TTF_FILE_SUPPORT 0
    #define LV_TINY_TTF_CACHE_GLYPH_CNT 128
#endif

/* Display driver for the Linux simulator */
#define LV_USE_SDL 1
#if LV_USE_SDL
    #define LV_SDL_INCLUDE_PATH     <SDL2/SDL.h>
    #define LV_SDL_RENDER_MODE      LV_DISPLAY_RENDER_MODE_DIRECT
    #define LV_SDL_BUF_COUNT        1
    #define LV_SDL_ACCELERATED      1
    #define LV_SDL_FULLSCREEN       0
    #define LV_SDL_DIRECT_EXIT      1
    #define LV_SDL_MOUSEWHEEL_MODE  LV_SDL_MOUSEWHEEL_MODE_ENCODER
#endif

/* Unused features */
#define LV_USE_DEMO_WIDGETS 0
#define LV_BUILD_EXAMPLES 0

#endif /* LV_CONF_H */
