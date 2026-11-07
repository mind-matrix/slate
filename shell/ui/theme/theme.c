#include "theme/theme.h"

#include <math.h>

LV_FONT_DECLARE(slate_font_14)
LV_FONT_DECLARE(slate_font_16)
LV_FONT_DECLARE(slate_font_20)
LV_FONT_DECLARE(slate_font_24)
LV_FONT_DECLARE(slate_font_32)
LV_FONT_DECLARE(slate_font_48)
LV_FONT_DECLARE(slate_mono_18)
LV_FONT_DECLARE(slate_digits_96)

static lv_style_t screen_style;
static lv_style_t pressed_style;
static bool initialized;

void sui_theme_init(void)
{
    if (initialized)
        return;

    lv_style_init(&screen_style);
    lv_style_set_bg_color(&screen_style, lv_color_hex(SUI_COLOR_BG));
    lv_style_set_bg_opa(&screen_style, LV_OPA_COVER);
    lv_style_set_text_color(&screen_style, lv_color_hex(SUI_COLOR_TEXT));
    lv_style_set_text_font(&screen_style, sui_font(SUI_TEXT_BODY));

    lv_style_init(&pressed_style);
    lv_style_set_opa(&pressed_style, LV_OPA_70);

    initialized = true;
}

const lv_font_t *sui_font(sui_text_t text)
{
    switch (text) {
    case SUI_TEXT_CAPTION:
        return &slate_font_14;
    case SUI_TEXT_ROW:
        return &slate_font_20;
    case SUI_TEXT_HEADING:
        return &slate_font_24;
    case SUI_TEXT_TITLE:
        return &slate_font_32;
    case SUI_TEXT_DISPLAY:
        return &slate_font_48;
    case SUI_TEXT_BODY:
    default:
        return &slate_font_16;
    }
}

static double linear_channel(uint32_t color, int shift)
{
    double value = ((color >> shift) & 0xFF) / 255.0;
    return value <= 0.04045 ? value / 12.92 : pow((value + 0.055) / 1.055, 2.4);
}

static double luminance(uint32_t color)
{
    return 0.2126 * linear_channel(color, 16) + 0.7152 * linear_channel(color, 8) +
           0.0722 * linear_channel(color, 0);
}

double sui_contrast_ratio(uint32_t a, uint32_t b)
{
    double la = luminance(a);
    double lb = luminance(b);
    return la > lb ? (la + 0.05) / (lb + 0.05) : (lb + 0.05) / (la + 0.05);
}

lv_obj_t *sui_screen_create(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_add_style(screen, &screen_style, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    return screen;
}

lv_obj_t *sui_box_create(lv_obj_t *parent)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    return box;
}

const lv_style_t *sui_style_pressed(void)
{
    return &pressed_style;
}

const lv_font_t *sui_font_mono(void)
{
    return &slate_mono_18;
}

const lv_font_t *sui_font_digits(void)
{
    return &slate_digits_96;
}
