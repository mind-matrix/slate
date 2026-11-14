#ifndef SUI_COMPONENTS_STATUS_BAR_H
#define SUI_COMPONENTS_STATUS_BAR_H

#include <stdbool.h>
#include "lvgl.h"

lv_obj_t *sui_status_bar_create(lv_obj_t *parent, void (*on_pull)(void));
void sui_status_bar_set_radios(bool wifi, bool bluetooth);
void sui_status_bar_set_alerts(bool notifications, bool unseen, bool alarm);
/* System Back and Home at the bar's left. Over a full-screen app the bar also gets a background and
 * drops its pull handle. */
void sui_status_bar_set_controls(bool show_back, bool show_home, bool over_app, void (*on_back)(void),
                                 void (*on_home)(void));
/* The assistant with the microphone open, or NULL. Filled while it captures a request. */
void sui_status_bar_set_mic(const char *name, bool capturing, void (*on_tap)(void));

#endif
