#ifndef SUI_THEME_H
#define SUI_THEME_H

#include "lvgl.h"

#define SUI_COLOR_BG 0x121A2E
#define SUI_COLOR_SURFACE 0x1B2540
#define SUI_COLOR_RAISED 0x253052
#define SUI_COLOR_BORDER 0x313D63
#define SUI_COLOR_TRACK 0x3D4A72
#define SUI_COLOR_TEXT 0xECEEF1
#define SUI_COLOR_TEXT_MUTED 0xA9B3CC
#define SUI_COLOR_ACCENT 0x7AB0FF
#define SUI_COLOR_ACCENT_FILL 0x3B6FD6
#define SUI_COLOR_ON_ACCENT 0x121A2E
#define SUI_COLOR_SUCCESS 0x5FD39A
#define SUI_COLOR_DANGER 0xFF8A7A
#define SUI_COLOR_DANGER_FILL 0xD63B3B
#define SUI_COLOR_WARNING_FILL 0xFFC65C
#define SUI_COLOR_WHITE 0xFFFFFF
#define SUI_COLOR_CONSOLE 0x0B1122

#define SUI_COLOR_CAT_WIFI 0x7AB0FF
#define SUI_COLOR_CAT_BLUETOOTH 0xA99BFF
#define SUI_COLOR_CAT_SOUND 0xFF9FC8
#define SUI_COLOR_CAT_DISPLAY 0xFFC65C
#define SUI_COLOR_CAT_SYSTEM 0x5FD3C4
#define SUI_COLOR_CAT_TERMINAL 0x5FD39A

#define SUI_TINT_OPA 41

#define SUI_SPACE_XS 4
#define SUI_SPACE_S 8
#define SUI_SPACE_M 16
#define SUI_SPACE_L 24
#define SUI_SPACE_XL 32

#define SUI_RADIUS_BUTTON 12
#define SUI_RADIUS_ROW 16
#define SUI_RADIUS_TILE 20
#define SUI_RADIUS_DIALOG 24
#define SUI_RADIUS_ICON 28

#define SUI_TOUCH_MIN 48
#define SUI_TOUCH 56
#define SUI_ROW_H 72
#define SUI_ROW_TALL_H 88
#define SUI_CONTENT_W 880
#define SUI_STATUS_BAR_H 48
#define SUI_HEADER_H 80

typedef enum {
    SUI_TEXT_CAPTION,
    SUI_TEXT_BODY,
    SUI_TEXT_ROW,
    SUI_TEXT_HEADING,
    SUI_TEXT_TITLE,
    SUI_TEXT_DISPLAY,
} sui_text_t;

void sui_theme_init(void);
const lv_font_t *sui_font(sui_text_t text);
const lv_font_t *sui_font_mono(void);
double sui_contrast_ratio(uint32_t a, uint32_t b);
lv_obj_t *sui_screen_create(void);
lv_obj_t *sui_box_create(lv_obj_t *parent);
const lv_style_t *sui_style_pressed(void);

#endif
