#ifndef SUI_BACKEND_H
#define SUI_BACKEND_H

#include "platform/platform.h"

lv_display_t *sui_backend_open(void);
lv_display_t *sui_headless_open(void);
/* Feeds one evdev key event (value 1 press, 0 release, 2 repeat) to the shell. */
void sui_keyboard_key(unsigned int code, int value);

#endif
