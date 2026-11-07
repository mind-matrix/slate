#ifndef SUI_BACKEND_H
#define SUI_BACKEND_H

#include "platform/platform.h"

lv_display_t *sui_backend_open(void);
lv_display_t *sui_headless_open(void);
lv_indev_t *sui_touch_open(lv_display_t *disp);

#endif
