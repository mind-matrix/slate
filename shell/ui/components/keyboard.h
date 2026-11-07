#ifndef SUI_COMPONENTS_KEYBOARD_H
#define SUI_COMPONENTS_KEYBOARD_H

#include "lvgl.h"
#include "platform/platform.h"

#define SUI_KEY_HIDE 0xE100

typedef enum {
    SUI_KEYBOARD_TEXT,
    SUI_KEYBOARD_TERMINAL,
} sui_keyboard_mode_t;

lv_obj_t *sui_keyboard_create(lv_obj_t *parent, sui_keyboard_mode_t mode, sui_key_cb_t cb, void *user);

#endif
