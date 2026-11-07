#include "components/roller.h"

#include <stdio.h>
#include "theme/theme.h"

#define MAX_VALUE 99
#define VISIBLE_ROWS 3
#define LINE_SPACE 18

lv_obj_t *sui_roller_create(lv_obj_t *parent, int max, int32_t width)
{
    char options[(MAX_VALUE + 1) * 3 + 1];
    size_t used = 0;
    if (max > MAX_VALUE)
        max = MAX_VALUE;
    for (int i = 0; i <= max; i++)
        used += (size_t)snprintf(options + used, sizeof(options) - used, i ? "\n%02d" : "%02d", i);

    lv_obj_t *roller = lv_roller_create(parent);
    lv_obj_set_style_text_font(roller, sui_font(SUI_TEXT_DISPLAY), 0);
    lv_obj_set_style_text_line_space(roller, LINE_SPACE, 0);
    lv_roller_set_options(roller, options, LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(roller, width);
    lv_obj_set_style_text_align(roller, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(roller, lv_color_hex(SUI_COLOR_TRACK), 0);
    lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(roller, 0, 0);
    lv_obj_set_style_pad_all(roller, 0, 0);
    lv_obj_set_style_text_font(roller, sui_font(SUI_TEXT_DISPLAY), LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller, lv_color_hex(SUI_COLOR_TEXT), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(roller, lv_color_hex(SUI_COLOR_RAISED), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_radius(roller, SUI_RADIUS_ROW, LV_PART_SELECTED);
    lv_roller_set_visible_row_count(roller, VISIBLE_ROWS);
    return roller;
}

void sui_roller_set(lv_obj_t *roller, int value)
{
    lv_roller_set_selected(roller, (uint32_t)value, LV_ANIM_OFF);
}

int sui_roller_get(lv_obj_t *roller)
{
    return (int)lv_roller_get_selected(roller);
}
