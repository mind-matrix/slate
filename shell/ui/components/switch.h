#ifndef SUI_COMPONENTS_SWITCH_H
#define SUI_COMPONENTS_SWITCH_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

lv_obj_t *sui_switch_create(lv_obj_t *parent, uint32_t color);
void sui_switch_set(lv_obj_t *sw, bool on);
bool sui_switch_get(lv_obj_t *sw);

#endif
