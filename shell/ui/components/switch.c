#include "components/switch.h"

#include "theme/theme.h"

#define SWITCH_W 64
#define SWITCH_H 36
#define KNOB_INSET 3
#define TOUCH_EXTRA 12

lv_obj_t *sui_switch_create(lv_obj_t *parent, uint32_t color)
{
    lv_obj_t *sw = lv_switch_create(parent);
    lv_obj_set_size(sw, SWITCH_W, SWITCH_H);
    lv_obj_set_ext_click_area(sw, TOUCH_EXTRA);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(SUI_COLOR_TRACK), LV_PART_MAIN);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(sw, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, lv_color_hex(color), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(sw, lv_color_hex(SUI_COLOR_TEXT_MUTED), LV_PART_KNOB);
    lv_obj_set_style_bg_color(sw, lv_color_hex(SUI_COLOR_BG), LV_PART_KNOB | LV_STATE_CHECKED);
    lv_obj_set_style_pad_all(sw, -KNOB_INSET, LV_PART_KNOB);
    lv_obj_set_style_anim_duration(sw, 120, LV_PART_MAIN);
    return sw;
}

void sui_switch_set(lv_obj_t *sw, bool on)
{
    if (on)
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    else
        lv_obj_remove_state(sw, LV_STATE_CHECKED);
}

bool sui_switch_get(lv_obj_t *sw)
{
    return lv_obj_has_state(sw, LV_STATE_CHECKED);
}
