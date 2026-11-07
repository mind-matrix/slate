#include "components/app_icon.h"

#include "theme/theme.h"

#define TILE_SIZE 112
#define TILE_GAP 14

lv_obj_t *sui_app_icon_create(lv_obj_t *parent, const char *glyph, const char *label, uint32_t color)
{
    lv_obj_t *icon = sui_box_create(parent);
    lv_obj_add_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(icon, SUI_APP_ICON_W, SUI_APP_ICON_H);
    lv_obj_set_flex_flow(icon, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(icon, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(icon, TILE_GAP, 0);
    lv_obj_add_style(icon, sui_style_pressed(), LV_STATE_PRESSED);

    lv_obj_t *tile = sui_box_create(icon);
    lv_obj_set_size(tile, TILE_SIZE, TILE_SIZE);
    lv_obj_set_style_radius(tile, SUI_RADIUS_ICON, 0);
    lv_obj_set_style_bg_color(tile, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(tile, SUI_TINT_OPA, 0);

    lv_obj_t *mark = lv_label_create(tile);
    lv_label_set_text(mark, glyph);
    lv_obj_set_style_text_font(mark, sui_font(SUI_TEXT_DISPLAY), 0);
    lv_obj_set_style_text_color(mark, lv_color_hex(color), 0);
    lv_obj_center(mark);

    lv_obj_t *name = lv_label_create(icon);
    lv_label_set_text(name, label);
    lv_obj_set_style_text_font(name, sui_font(SUI_TEXT_ROW), 0);
    lv_obj_set_style_text_color(name, lv_color_hex(SUI_COLOR_TEXT), 0);
    return icon;
}
