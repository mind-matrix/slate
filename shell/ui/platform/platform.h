#ifndef SUI_PLATFORM_H
#define SUI_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#define SUI_SCREEN_W 1280
#define SUI_SCREEN_H 720

enum {
    SUI_KEY_ENTER = 0xE000,
    SUI_KEY_BACKSPACE,
    SUI_KEY_TAB,
    SUI_KEY_ESC,
    SUI_KEY_UP,
    SUI_KEY_DOWN,
    SUI_KEY_LEFT,
    SUI_KEY_RIGHT,
    SUI_KEY_HOME,
    SUI_KEY_END,
    SUI_KEY_PAGE_UP,
    SUI_KEY_PAGE_DOWN,
    SUI_KEY_DELETE,
};

#define SUI_MOD_CTRL 1u
#define SUI_MOD_ALT 2u

typedef void (*sui_fd_cb_t)(int fd, void *user);
typedef void (*sui_key_cb_t)(uint32_t key, uint32_t mods, void *user);
typedef bool (*sui_wake_cb_t)(void);
typedef void (*sui_hook_t)(void);

lv_display_t *sui_platform_init(void);
lv_display_t *sui_headless_init(void);
const uint16_t *sui_headless_pixels(void);
bool sui_watch_fd(int fd, sui_fd_cb_t cb, void *user);
void sui_unwatch_fd(int fd);
void sui_set_key_handler(sui_key_cb_t cb, void *user);
void sui_key_dispatch(uint32_t key, uint32_t mods);
/* The idle screen registers a handler that wakes the display. Input that only wakes the
 * screen is not delivered anywhere else. */
void sui_set_wake_handler(sui_wake_cb_t cb);
bool sui_wake(void);
/* Runs before the main loop waits, so the compositor can flush its clients. */
void sui_set_poll_hook(sui_hook_t hook);
void sui_run(void);
void sui_run_for(uint32_t ms);
void sui_quit(void);

#endif
