#include "components/button.h"

#include "theme/theme.h"

#define BUTTON_PAD_HOR 24

lv_obj_t *sui_button_create(lv_obj_t *parent, const char *text, uint32_t fill, uint32_t ink)
{
    lv_obj_t *button = sui_box_create(parent);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(button, SUI_TOUCH);
    lv_obj_set_style_pad_hor(button, BUTTON_PAD_HOR, 0);
    lv_obj_set_style_radius(button, SUI_RADIUS_BUTTON, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(fill), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(button, lv_color_hex(ink), 0);
    lv_obj_set_style_text_font(button, sui_font(SUI_TEXT_BODY), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(SUI_COLOR_RAISED), LV_STATE_DISABLED);
    lv_obj_set_style_text_color(button, lv_color_hex(SUI_COLOR_TEXT_MUTED), LV_STATE_DISABLED);
    lv_obj_add_style(button, sui_style_pressed(), LV_STATE_PRESSED);
    lv_obj_set_flex_flow(button, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(button, SUI_SPACE_M, 0);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_user_data(button, label);
    return button;
}

lv_obj_t *sui_button_secondary_create(lv_obj_t *parent, const char *text)
{
    return sui_button_create(parent, text, SUI_COLOR_RAISED, SUI_COLOR_TEXT);
}

void sui_button_set_text(lv_obj_t *button, const char *text)
{
    lv_label_set_text(lv_obj_get_user_data(button), text);
}
