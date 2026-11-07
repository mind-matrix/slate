#include "components/stat_card.h"

#include "theme/theme.h"

#define CARD_PAD_HOR 28
#define CARD_PAD_VER 24
#define BAR_H 10
#define VALUE_GAP 8

typedef struct {
    lv_obj_t *value;
    lv_obj_t *unit;
    lv_obj_t *bar;
    lv_obj_t *stats[SUI_STAT_COUNT];
} stat_card_t;

static lv_obj_t *add_label(lv_obj_t *parent, const char *text, sui_text_t style, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, sui_font(style), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

static void free_parts(lv_event_t *e)
{
    lv_free(lv_obj_get_user_data(lv_event_get_target(e)));
}

static lv_obj_t *create_bar(lv_obj_t *parent, uint32_t color)
{
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_set_size(bar, LV_PCT(100), BAR_H);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_radius(bar, BAR_H / 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(SUI_COLOR_RAISED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, BAR_H / 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    return bar;
}

lv_obj_t *sui_stat_card_create(lv_obj_t *parent, const char *title, const char *const keys[SUI_STAT_COUNT],
                               uint32_t color)
{
    stat_card_t *parts = lv_malloc_zeroed(sizeof(*parts));
    lv_obj_t *card = sui_box_create(parent);
    lv_obj_set_user_data(card, parts);
    lv_obj_add_event_cb(card, free_parts, LV_EVENT_DELETE, NULL);
    lv_obj_set_style_pad_hor(card, CARD_PAD_HOR, 0);
    lv_obj_set_style_pad_ver(card, CARD_PAD_VER, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(SUI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(SUI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, SUI_RADIUS_TILE, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *top = sui_box_create(card);
    lv_obj_set_width(top, LV_PCT(100));
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(top, VALUE_GAP, 0);
    lv_obj_t *name = add_label(top, title, SUI_TEXT_ROW, SUI_COLOR_TEXT);
    lv_obj_set_flex_grow(name, 1);
    parts->value = add_label(top, "—", SUI_TEXT_TITLE, SUI_COLOR_TEXT);
    parts->unit = add_label(top, "", SUI_TEXT_BODY, SUI_COLOR_TEXT_MUTED);

    parts->bar = create_bar(card, color);

    lv_obj_t *grid = sui_box_create(card);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW);
    for (int i = 0; i < SUI_STAT_COUNT; i++) {
        lv_obj_t *cell = sui_box_create(grid);
        lv_obj_set_flex_grow(cell, 1);
        lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(cell, SUI_SPACE_XS, 0);
        add_label(cell, keys[i], SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);
        parts->stats[i] = add_label(cell, "—", SUI_TEXT_BODY, SUI_COLOR_TEXT);
    }
    return card;
}

void sui_stat_card_set(lv_obj_t *card, const char *value, const char *unit, int32_t percent,
                       const char *const values[SUI_STAT_COUNT])
{
    stat_card_t *parts = lv_obj_get_user_data(card);
    lv_label_set_text(parts->value, value);
    lv_label_set_text(parts->unit, unit);
    lv_bar_set_value(parts->bar, LV_CLAMP(0, percent, 100), LV_ANIM_OFF);
    for (int i = 0; i < SUI_STAT_COUNT; i++)
        lv_label_set_text(parts->stats[i], values[i]);
}
