/* Intentionally empty: replaces LVGL's ESP_PLATFORM includes (sdkconfig.h, esp_attr.h) in
 * lv_conf_kconfig.h, which break LVGL's assembler sources under PlatformIO. LVGL is configured
 * via lv_conf.h only. Selected with LV_CONF_KCONFIG_EXTERNAL_INCLUDE in platformio.ini. */
