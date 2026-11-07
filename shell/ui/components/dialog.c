#include "components/dialog.h"

#include "theme/theme.h"

#define DIALOG_W 600
#define DIALOG_PAD 36
#define SCRIM_COLOR 0x080C18
#define SCRIM_OPA 184
#define ACTIONS_GAP 12

lv_obj_t *sui_dialog_create(const char *title, const char *body)
{
    lv_obj_t *scrim = sui_box_create(lv_layer_top());
    lv_obj_add_flag(scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(scrim, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(scrim, lv_color_hex(SCRIM_COLOR), 0);
    lv_obj_set_style_bg_opa(scrim, SCRIM_OPA, 0);

    lv_obj_t *dialog = sui_box_create(scrim);
    lv_obj_add_flag(dialog, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_width(dialog, DIALOG_W);
    lv_obj_center(dialog);
    lv_obj_set_style_pad_all(dialog, DIALOG_PAD, 0);
    lv_obj_set_style_pad_row(dialog, SUI_SPACE_M, 0);
    lv_obj_set_style_radius(dialog, SUI_RADIUS_DIALOG, 0);
    lv_obj_set_style_bg_color(dialog, lv_color_hex(SUI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(dialog, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(dialog, lv_color_hex(SUI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(dialog, 1, 0);
    lv_obj_set_flex_flow(dialog, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *heading = lv_label_create(dialog);
    lv_label_set_text(heading, title);
    lv_obj_set_style_text_font(heading, sui_font(SUI_TEXT_TITLE), 0);
    lv_obj_set_style_text_color(heading, lv_color_hex(SUI_COLOR_TEXT), 0);

    lv_obj_t *text = lv_label_create(dialog);
    lv_obj_set_width(text, LV_PCT(100));
    lv_label_set_text(text, body);
    lv_obj_set_style_text_font(text, sui_font(SUI_TEXT_ROW), 0);
    lv_obj_set_style_text_color(text, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_text_line_space(text, 6, 0);

    lv_obj_t *actions = sui_box_create(dialog);
    lv_obj_set_width(actions, LV_PCT(100));
    lv_obj_set_style_pad_top(actions, ACTIONS_GAP, 0);
    lv_obj_set_style_pad_column(actions, ACTIONS_GAP, 0);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_user_data(dialog, actions);
    return dialog;
}

lv_obj_t *sui_dialog_actions(lv_obj_t *dialog)
{
    return lv_obj_get_user_data(dialog);
}

void sui_dialog_close(lv_obj_t *dialog)
{
    lv_obj_delete_async(lv_obj_get_parent(dialog));
}
