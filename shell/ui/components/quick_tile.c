#include "components/quick_tile.h"

#include <string.h>
#include "theme/theme.h"

#define TILE_H 96
#define TILE_PAD_HOR 24
#define TILE_GAP 16
#define GLYPH_SIZE 30

lv_obj_t *sui_quick_tile_create(lv_obj_t *parent, sui_glyph_t glyph)
{
    lv_obj_t *tile = sui_box_create(parent);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(tile, TILE_H);
    lv_obj_set_flex_grow(tile, 1);
    lv_obj_set_style_pad_hor(tile, TILE_PAD_HOR, 0);
    lv_obj_set_style_pad_column(tile, TILE_GAP, 0);
    lv_obj_set_style_radius(tile, SUI_RADIUS_TILE, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(tile, lv_color_hex(SUI_COLOR_RAISED), 0);
    lv_obj_set_style_bg_color(tile, lv_color_hex(SUI_COLOR_ACCENT_FILL), LV_STATE_CHECKED);
    lv_obj_add_style(tile, sui_style_pressed(), LV_STATE_PRESSED);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    sui_glyph_create(tile, glyph, GLYPH_SIZE, SUI_COLOR_TEXT_MUTED);
    lv_obj_t *label = lv_label_create(tile);
    lv_obj_set_flex_grow(label, 1);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(label, sui_font(SUI_TEXT_ROW), 0);
    sui_quick_tile_set(tile, false, "Off");
    return tile;
}

void sui_quick_tile_set(lv_obj_t *tile, bool on, const char *text)
{
    lv_color_t ink = lv_color_hex(on ? SUI_COLOR_WHITE : SUI_COLOR_TEXT_MUTED);
    lv_obj_t *glyph = lv_obj_get_child(tile, 0);
    lv_obj_t *label = lv_obj_get_child(tile, 1);

    if (on)
        lv_obj_add_state(tile, LV_STATE_CHECKED);
    else
        lv_obj_remove_state(tile, LV_STATE_CHECKED);
    lv_obj_set_style_text_color(glyph, ink, 0);
    lv_obj_set_style_line_color(glyph, ink, 0);
    lv_obj_set_style_text_color(label, ink, 0);
    if (strcmp(lv_label_get_text(label), text) != 0)
        lv_label_set_text(label, text);
}

bool sui_quick_tile_is_on(lv_obj_t *tile)
{
    return lv_obj_has_state(tile, LV_STATE_CHECKED);
}
