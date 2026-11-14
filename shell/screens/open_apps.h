#ifndef SLATE_OPEN_APPS_H
#define SLATE_OPEN_APPS_H

#include "lvgl.h"

/* The drawer's "Open apps" section: apps with a window, in front or behind Home. Tapping one brings it
 * back; ✕ closes (stops) it. Always-on apps (the voice assistants) are listed without ✕ and are left
 * alone by Close all; they are stopped from their app details instead. on_open runs after an app is
 * brought back, so the drawer can close. */
lv_obj_t *open_apps_create(lv_obj_t *parent, void (*on_open)(void));
void open_apps_refresh(lv_obj_t *section);

#endif
