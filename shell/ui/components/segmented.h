#ifndef SUI_COMPONENTS_SEGMENTED_H
#define SUI_COMPONENTS_SEGMENTED_H

#include <stddef.h>
#include <stdint.h>
#include "lvgl.h"

lv_obj_t *sui_segmented_create(lv_obj_t *parent, const char *const *labels, size_t count, uint32_t color);
void sui_segmented_set(lv_obj_t *segmented, size_t index);
size_t sui_segmented_get(lv_obj_t *segmented);

#endif
