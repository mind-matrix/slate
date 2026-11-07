#include "settings_ui.h"

#include "components/button.h"
#include "components/dialog.h"
#include "components/header.h"
#include "components/switch.h"
#include "theme/theme.h"

#define CARD_H 72
#define CARD_PAD_LEFT 24
#define CARD_PAD_RIGHT 16
#define SECTION_GAP 10
#define SECTION_INSET 8
#define EMPTY_GLYPH 48

lv_obj_t *settings_page_create(lv_obj_t *screen, const char *title)
{
    lv_obj_t *page = sui_page_create(screen, title);
    lv_obj_add_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(page, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    return page;
}

lv_obj_t *settings_card_create(lv_obj_t *parent)
{
    lv_obj_t *card = sui_box_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_style_bg_color(card, lv_color_hex(SUI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(SUI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, SUI_RADIUS_ROW, 0);
    return card;
}

lv_obj_t *settings_switch_card_create(lv_obj_t *parent, const char *title, const char *detail, uint32_t color)
{
    lv_obj_t *card = settings_card_create(parent);
    lv_obj_set_height(card, CARD_H);
    lv_obj_set_style_pad_left(card, CARD_PAD_LEFT, 0);
    lv_obj_set_style_pad_right(card, CARD_PAD_RIGHT, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *text = sui_box_create(card);
    lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(text, 2, 0);
    lv_obj_t *label = lv_label_create(text);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, sui_font(SUI_TEXT_ROW), 0);
    if (detail) {
        lv_obj_t *sub = lv_label_create(text);
        lv_label_set_text(sub, detail);
        lv_obj_set_style_text_font(sub, sui_font(SUI_TEXT_CAPTION), 0);
        lv_obj_set_style_text_color(sub, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    }
    return sui_switch_create(card, color);
}

lv_obj_t *settings_section_create(lv_obj_t *parent, const char *title, lv_obj_t **status)
{
    lv_obj_t *section = sui_box_create(parent);
    lv_obj_set_width(section, LV_PCT(100));
    lv_obj_set_style_pad_row(section, SECTION_GAP, 0);
    lv_obj_set_flex_flow(section, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *head = sui_box_create(section);
    lv_obj_set_width(head, LV_PCT(100));
    lv_obj_set_style_pad_hor(head, SECTION_INSET, 0);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *label = lv_label_create(head);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, sui_font(SUI_TEXT_CAPTION), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_text_letter_space(label, 1, 0);

    if (status) {
        *status = lv_label_create(head);
        lv_label_set_text(*status, "");
        lv_obj_set_style_text_font(*status, sui_font(SUI_TEXT_CAPTION), 0);
        lv_obj_set_style_text_color(*status, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    }
    return section;
}

lv_obj_t *settings_empty_state_create(lv_obj_t *parent, sui_glyph_t glyph, const char *title, const char *detail)
{
    lv_obj_t *empty = sui_box_create(parent);
    lv_obj_set_width(empty, LV_PCT(100));
    lv_obj_set_flex_grow(empty, 1);
    lv_obj_set_style_pad_row(empty, 12, 0);
    lv_obj_set_flex_flow(empty, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(empty, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    sui_glyph_create(empty, glyph, EMPTY_GLYPH, SUI_COLOR_TEXT_MUTED);
    lv_obj_t *heading = lv_label_create(empty);
    lv_label_set_text(heading, title);
    lv_obj_set_style_text_font(heading, sui_font(SUI_TEXT_ROW), 0);
    lv_obj_t *text = lv_label_create(empty);
    lv_label_set_text(text, detail);
    lv_obj_set_style_text_color(text, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    return empty;
}

void settings_set_visible(lv_obj_t *obj, bool visible)
{
    if (visible)
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void close_dialog(lv_event_t *e)
{
    sui_dialog_close(lv_event_get_user_data(e));
}

lv_obj_t *settings_info_dialog(const char *title, const char *body)
{
    lv_obj_t *dialog = sui_dialog_create(title, body);
    lv_obj_t *ok = sui_button_create(sui_dialog_actions(dialog), "OK", SUI_COLOR_ACCENT, SUI_COLOR_ON_ACCENT);
    lv_obj_add_event_cb(ok, close_dialog, LV_EVENT_CLICKED, dialog);
    return dialog;
}
