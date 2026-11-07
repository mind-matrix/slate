#ifndef SUI_COMPONENTS_SLIDER_ROW_H
#define SUI_COMPONENTS_SLIDER_ROW_H

#include <stdbool.h>
#include <stdint.h>
#include "components/icon.h"
#include "lvgl.h"

lv_obj_t *sui_slider_row_create(lv_obj_t *parent, sui_glyph_t glyph, uint32_t color, int32_t min, int32_t max);
lv_obj_t *sui_slider_row_slider(lv_obj_t *row);
void sui_slider_row_set_value(lv_obj_t *row, int32_t value);
void sui_slider_row_set_available(lv_obj_t *row, bool available);

#endif
