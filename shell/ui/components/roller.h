#ifndef SUI_COMPONENTS_ROLLER_H
#define SUI_COMPONENTS_ROLLER_H

#include "lvgl.h"

lv_obj_t *sui_roller_create(lv_obj_t *parent, int max, int32_t width);
void sui_roller_set(lv_obj_t *roller, int value);
int sui_roller_get(lv_obj_t *roller);

#endif
