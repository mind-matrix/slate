#include "components/list.h"

#include <string.h>
#include "theme/theme.h"

#define ROW_PAD_HOR 24
#define ROW_GAP 20
#define ICON_TILE 48
#define ICON_SIZE 26
#define ICON_RADIUS 14
#define SECTION_PAD_LEFT 8
#define SECTION_LETTER_SPACE 1

lv_obj_t *sui_list_create(lv_obj_t *parent, int32_t width)
{
    lv_obj_t *list = sui_box_create(parent);
    lv_obj_set_width(list, width);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(list, lv_color_hex(SUI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(list, lv_color_hex(SUI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(list, 1, 0);
    lv_obj_set_style_radius(list, SUI_RADIUS_ROW, 0);
    lv_obj_set_style_clip_corner(list, true, 0);
    return list;
}

static void add_divider_if_needed(lv_obj_t *row)
{
    if (lv_obj_get_index(row) == 0)
        return;
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(SUI_COLOR_BORDER), 0);
}

static lv_obj_t *add_label(lv_obj_t *parent, const char *text, sui_text_t style, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, sui_font(style), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

lv_obj_t *sui_list_row_create(lv_obj_t *list, const sui_row_t *spec)
{
    bool has_icon = spec->glyph.symbol || spec->glyph.icon;
    bool has_subtitle = spec->detail && spec->trailing != SUI_TRAILING_VALUE;
    lv_obj_t *row = sui_box_create(list);
    lv_obj_set_size(row, LV_PCT(100), has_icon || has_subtitle ? SUI_ROW_TALL_H : SUI_ROW_H);
    lv_obj_set_style_pad_hor(row, ROW_PAD_HOR, 0);
    lv_obj_set_style_pad_column(row, ROW_GAP, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    add_divider_if_needed(row);

    if (spec->trailing == SUI_TRAILING_CHEVRON) {
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(row, lv_color_hex(SUI_COLOR_RAISED), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
    }

    if (has_icon) {
        lv_obj_t *tile = sui_box_create(row);
        lv_obj_set_size(tile, ICON_TILE, ICON_TILE);
        lv_obj_set_style_radius(tile, ICON_RADIUS, 0);
        lv_obj_set_style_bg_color(tile, lv_color_hex(spec->color), 0);
        lv_obj_set_style_bg_opa(tile, SUI_TINT_OPA, 0);
        lv_obj_center(sui_glyph_create(tile, spec->glyph, ICON_SIZE, spec->color));
    }

    lv_obj_t *text = sui_box_create(row);
    lv_obj_set_flex_grow(text, 1);
    lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(text, 2, 0);
    add_label(text, spec->title, SUI_TEXT_ROW, SUI_COLOR_TEXT);

    lv_obj_t *detail = NULL;
    if (spec->trailing == SUI_TRAILING_VALUE)
        detail = add_label(row, spec->detail ? spec->detail : "", SUI_TEXT_BODY, SUI_COLOR_TEXT_MUTED);
    else if (spec->detail)
        detail = add_label(text, spec->detail, SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);
    lv_obj_set_user_data(row, detail);

    if (spec->trailing == SUI_TRAILING_CHEVRON)
        add_label(row, LV_SYMBOL_RIGHT, SUI_TEXT_ROW, SUI_COLOR_TEXT_MUTED);
    return row;
}

void sui_list_row_set_detail(lv_obj_t *row, const char *text)
{
    lv_obj_t *detail = lv_obj_get_user_data(row);
    if (detail && strcmp(lv_label_get_text(detail), text) != 0)
        lv_label_set_text(detail, text);
}

lv_obj_t *sui_section_label_create(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = add_label(parent, text, SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);
    lv_obj_set_style_pad_left(label, SECTION_PAD_LEFT, 0);
    lv_obj_set_style_text_letter_space(label, SECTION_LETTER_SPACE, 0);
    return label;
}
