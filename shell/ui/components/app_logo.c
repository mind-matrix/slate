#include "components/app_logo.h"

#include <stdio.h>
#include "components/app_icon.h"
#include "theme/theme.h"

#define SOURCE_MAX 192
#define ICON_TILE 112
#define ICON_GAP 14

static bool usable(const char *source)
{
    lv_image_header_t header;
    return lv_image_decoder_get_info(source, &header) == LV_RESULT_OK && header.w == SUI_APP_LOGO_SOURCE &&
           header.h == SUI_APP_LOGO_SOURCE;
}

lv_obj_t *sui_app_logo_create(lv_obj_t *parent, const char *path, const char *name, int32_t size)
{
    lv_obj_t *tile = sui_box_create(parent);
    lv_obj_set_size(tile, size, size);
    lv_obj_set_style_radius(tile, size * 2 / 7, 0);
    lv_obj_set_style_clip_corner(tile, true, 0);

    char source[SOURCE_MAX];
    snprintf(source, sizeof(source), "A:%s", path ? path : "");
    if (path && path[0] && usable(source)) {
        lv_obj_t *image = lv_image_create(tile);
        lv_image_set_src(image, source);
        lv_image_set_pivot(image, 0, 0);
        lv_image_set_scale(image, (uint32_t)(LV_SCALE_NONE * size / SUI_APP_LOGO_SOURCE));
        lv_obj_set_pos(image, 0, 0);
        return tile;
    }

    char letter[2] = { name && name[0] ? (char)(name[0] >= 'a' && name[0] <= 'z' ? name[0] - 32 : name[0]) : '?',
                       '\0' };
    lv_obj_set_style_bg_color(tile, lv_color_hex(SUI_COLOR_RAISED), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_t *label = lv_label_create(tile);
    lv_label_set_text(label, letter);
    lv_obj_set_style_text_font(label, sui_font(size >= 96 ? SUI_TEXT_DISPLAY : SUI_TEXT_HEADING), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    lv_obj_center(label);
    return tile;
}

lv_obj_t *sui_app_icon_logo_create(lv_obj_t *parent, const char *path, const char *name)
{
    lv_obj_t *icon = sui_box_create(parent);
    lv_obj_add_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(icon, SUI_APP_ICON_W, SUI_APP_ICON_H);
    lv_obj_set_flex_flow(icon, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(icon, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(icon, ICON_GAP, 0);
    lv_obj_add_style(icon, sui_style_pressed(), LV_STATE_PRESSED);
    sui_app_logo_create(icon, path, name, ICON_TILE);
    lv_obj_t *label = lv_label_create(icon);
    lv_label_set_text(label, name);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label, SUI_APP_ICON_W);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, sui_font(SUI_TEXT_ROW), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(SUI_COLOR_TEXT), 0);
    return icon;
}
