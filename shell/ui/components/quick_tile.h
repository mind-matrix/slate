#ifndef SUI_COMPONENTS_QUICK_TILE_H
#define SUI_COMPONENTS_QUICK_TILE_H

#include <stdbool.h>
#include "components/icon.h"
#include "lvgl.h"

lv_obj_t *sui_quick_tile_create(lv_obj_t *parent, sui_glyph_t glyph);
void sui_quick_tile_set(lv_obj_t *tile, bool on, const char *text);
bool sui_quick_tile_is_on(lv_obj_t *tile);

#endif
