#ifndef SUI_COMPONENTS_STATUS_BAR_H
#define SUI_COMPONENTS_STATUS_BAR_H

#include <stdbool.h>
#include "lvgl.h"

lv_obj_t *sui_status_bar_create(lv_obj_t *parent, void (*on_pull)(void));
void sui_status_bar_set_radios(bool wifi, bool bluetooth);
void sui_status_bar_set_alerts(bool notifications, bool unseen, bool alarm);
/* While an app is open the bar gets a background and system Back and Home at its left. */
void sui_status_bar_set_app(bool open, void (*on_back)(void), void (*on_home)(void));
/* The assistant with the microphone open, or NULL. Filled while it captures a request. */
void sui_status_bar_set_mic(const char *name, bool capturing, void (*on_tap)(void));

#endif
