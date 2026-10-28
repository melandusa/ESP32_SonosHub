#include "ui/Styles.h"

namespace ui::styles {

void applyButtonStyle(lv_obj_t* button, lv_color_t color) {
    lv_obj_remove_style_all(button);
    lv_obj_set_style_radius(button, 14, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(button, color, 0);
    lv_obj_set_style_text_color(button, text(), 0);
    lv_obj_set_style_pad_all(button, 8, 0);

    // "Pressed in" feedback
    lv_obj_set_style_bg_color(button, lv_color_darken(color, LV_OPA_30), LV_STATE_PRESSED);
    lv_obj_set_style_translate_y(button, 3, LV_STATE_PRESSED);

    lv_obj_set_style_opa(button, LV_OPA_40, LV_STATE_DISABLED);
}

} // namespace ui::styles
