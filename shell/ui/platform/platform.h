#ifndef SUI_PLATFORM_H
#define SUI_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#define SUI_SCREEN_W 1280
#define SUI_SCREEN_H 720

typedef void (*sui_fd_cb_t)(int fd, void *user);

lv_display_t *sui_platform_init(void);
lv_display_t *sui_headless_init(void);
const uint16_t *sui_headless_pixels(void);
bool sui_watch_fd(int fd, sui_fd_cb_t cb, void *user);
void sui_run(void);
void sui_quit(void);

#endif
