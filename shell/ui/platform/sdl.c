#include "platform/backend.h"

typedef struct {
    lv_indev_read_cb_t read;
    bool pressed;
} sdl_keys_t;

static sdl_keys_t keys;

static uint32_t map_key(uint32_t key)
{
    switch (key) {
    case LV_KEY_ENTER:
        return SUI_KEY_ENTER;
    case LV_KEY_BACKSPACE:
        return SUI_KEY_BACKSPACE;
    case LV_KEY_NEXT:
        return SUI_KEY_TAB;
    case LV_KEY_ESC:
        return SUI_KEY_ESC;
    case LV_KEY_UP:
        return SUI_KEY_UP;
    case LV_KEY_DOWN:
        return SUI_KEY_DOWN;
    case LV_KEY_LEFT:
        return SUI_KEY_LEFT;
    case LV_KEY_RIGHT:
        return SUI_KEY_RIGHT;
    case LV_KEY_HOME:
        return SUI_KEY_HOME;
    case LV_KEY_END:
        return SUI_KEY_END;
    case LV_KEY_DEL:
        return SUI_KEY_DELETE;
    default:
        return key >= ' ' ? key : 0;
    }
}

static void read_keys(lv_indev_t *indev, lv_indev_data_t *data)
{
    keys.read(indev, data);
    bool pressed = data->state == LV_INDEV_STATE_PRESSED;
    uint32_t key = pressed && !keys.pressed ? map_key(data->key) : 0;
    keys.pressed = pressed;
    data->state = LV_INDEV_STATE_RELEASED;
    if (key)
        sui_key_dispatch(key, 0);
}

lv_display_t *sui_backend_open(void)
{
    lv_display_t *disp = lv_sdl_window_create(SUI_SCREEN_W, SUI_SCREEN_H);
    lv_sdl_window_set_title(disp, "Slate");
    lv_sdl_mouse_create();

    lv_indev_t *keyboard = lv_sdl_keyboard_create();
    keys.read = lv_indev_get_read_cb(keyboard);
    lv_indev_set_read_cb(keyboard, read_keys);
    return disp;
}
