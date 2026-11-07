#include "components/slider_row.h"

#include "theme/theme.h"

#define ROW_H 96
#define ROW_PAD_HOR 24
#define ROW_GAP 20
#define GLYPH_SIZE 28
#define TRACK_H 8
#define KNOB_GROW 10
#define TOUCH_EXTRA 20
#define VALUE_W 64
#define TITLE_GLYPH_W 32

static lv_obj_t *value_label(lv_obj_t *row)
{
    return lv_obj_get_user_data(sui_slider_row_slider(row));
}

static void show_value(lv_obj_t *row)
{
    lv_obj_t *slider = sui_slider_row_slider(row);
    if (lv_obj_has_state(slider, LV_STATE_DISABLED))
        lv_label_set_text(value_label(row), "–");
    else
        lv_label_set_text_fmt(value_label(row), "%d%%", (int)lv_slider_get_value(slider));
}

static void on_change(lv_event_t *e)
{
    show_value(lv_event_get_user_data(e));
}

lv_obj_t *sui_slider_row_create(lv_obj_t *parent, sui_glyph_t glyph, uint32_t color, int32_t min, int32_t max)
{
    lv_obj_t *row = sui_box_create(parent);
    lv_obj_set_size(row, LV_PCT(100), ROW_H);
    lv_obj_set_style_pad_hor(row, ROW_PAD_HOR, 0);
    lv_obj_set_style_pad_column(row, ROW_GAP, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(SUI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(SUI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_radius(row, SUI_RADIUS_ROW, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    sui_glyph_create(row, glyph, GLYPH_SIZE, color);

    lv_obj_t *slider = lv_slider_create(row);
    lv_obj_set_height(slider, TRACK_H);
    lv_obj_set_flex_grow(slider, 1);
    lv_obj_set_ext_click_area(slider, TOUCH_EXTRA);
    lv_slider_set_range(slider, min, max);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(SUI_COLOR_TRACK), LV_PART_MAIN);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(slider, lv_color_hex(color), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, KNOB_GROW, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_INDICATOR | LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(slider, lv_color_hex(SUI_COLOR_TRACK), LV_PART_KNOB | LV_STATE_DISABLED);

    lv_obj_t *value = lv_label_create(row);
    lv_obj_set_width(value, VALUE_W);
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(value, sui_font(SUI_TEXT_ROW), 0);
    lv_obj_set_style_text_color(value, lv_color_hex(SUI_COLOR_TEXT), 0);

    lv_obj_set_user_data(row, slider);
    lv_obj_set_user_data(slider, value);
    lv_obj_add_event_cb(slider, on_change, LV_EVENT_VALUE_CHANGED, row);
    show_value(row);
    return row;
}

lv_obj_t *sui_slider_row_slider(lv_obj_t *row)
{
    return lv_obj_get_user_data(row);
}

lv_obj_t *sui_slider_row_value(lv_obj_t *row)
{
    return value_label(row);
}

void sui_slider_row_set_title(lv_obj_t *row, const char *title, const char *detail, int32_t width)
{
    lv_obj_t *texts = sui_box_create(row);
    lv_obj_set_size(texts, width, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(texts, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(texts, 1, 0);
    lv_obj_t *label = lv_label_create(texts);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, sui_font(SUI_TEXT_ROW), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(SUI_COLOR_TEXT), 0);
    if (detail) {
        label = lv_label_create(texts);
        lv_label_set_text(label, detail);
        lv_obj_set_style_text_font(label, sui_font(SUI_TEXT_CAPTION), 0);
        lv_obj_set_style_text_color(label, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    }
    lv_obj_move_to_index(texts, 1);
    lv_obj_t *glyph = lv_obj_get_child(row, 0);
    lv_obj_set_width(glyph, TITLE_GLYPH_W);
    lv_obj_set_style_text_align(glyph, LV_TEXT_ALIGN_CENTER, 0);
}

void sui_slider_row_set_value(lv_obj_t *row, int32_t value)
{
    lv_slider_set_value(sui_slider_row_slider(row), value, LV_ANIM_OFF);
    show_value(row);
}

void sui_slider_row_set_available(lv_obj_t *row, bool available)
{
    lv_obj_t *slider = sui_slider_row_slider(row);
    if (available)
        lv_obj_remove_state(slider, LV_STATE_DISABLED);
    else
        lv_obj_add_state(slider, LV_STATE_DISABLED);
    show_value(row);
}
