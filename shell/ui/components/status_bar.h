#ifndef SUI_COMPONENTS_STATUS_BAR_H
#define SUI_COMPONENTS_STATUS_BAR_H

#include <stdbool.h>
#include "lvgl.h"

lv_obj_t *sui_status_bar_create(lv_obj_t *parent, void (*on_pull)(void));
void sui_status_bar_set_radios(bool wifi, bool bluetooth);

#endif
