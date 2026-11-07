#include "components/header.h"

#include "nav/nav.h"
#include "theme/theme.h"

#define HEADER_PAD_LEFT 20
#define HEADER_GAP 12

static void go_back(lv_event_t *e)
{
    (void)e;
    sui_nav_pop();
}

lv_obj_t *sui_header_create(lv_obj_t *parent, const char *title)
{
    lv_obj_t *header = sui_box_create(parent);
    lv_obj_set_size(header, LV_PCT(100), SUI_HEADER_H);
    lv_obj_set_pos(header, 0, SUI_STATUS_BAR_H);
    lv_obj_set_style_pad_left(header, HEADER_PAD_LEFT, 0);
    lv_obj_set_style_pad_column(header, HEADER_GAP, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *back = sui_box_create(header);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(back, SUI_TOUCH, SUI_TOUCH);
    lv_obj_set_style_radius(back, SUI_RADIUS_ROW, 0);
    lv_obj_add_style(back, sui_style_pressed(), LV_STATE_PRESSED);
    lv_obj_add_event_cb(back, go_back, LV_EVENT_CLICKED, NULL);

    lv_obj_t *arrow = lv_label_create(back);
    lv_label_set_text(arrow, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(arrow, sui_font(SUI_TEXT_HEADING), 0);
    lv_obj_center(arrow);

    lv_obj_t *label = lv_label_create(header);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, sui_font(SUI_TEXT_TITLE), 0);
    return header;
}

lv_obj_t *sui_page_create(lv_obj_t *screen, const char *title)
{
    sui_header_create(screen, title);

    lv_obj_t *content = sui_box_create(screen);
    lv_obj_set_size(content, LV_PCT(100), lv_display_get_vertical_resolution(NULL) - SUI_STATUS_BAR_H - SUI_HEADER_H);
    lv_obj_set_pos(content, 0, SUI_STATUS_BAR_H + SUI_HEADER_H);
    lv_obj_set_style_pad_top(content, SUI_SPACE_S, 0);
    lv_obj_set_style_pad_hor(content, SUI_SPACE_XL, 0);
    lv_obj_set_style_pad_bottom(content, SUI_SPACE_XL, 0);
    lv_obj_set_style_pad_row(content, SUI_SPACE_L - 4, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return content;
}
