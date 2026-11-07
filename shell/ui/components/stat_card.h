#ifndef SUI_COMPONENTS_STAT_CARD_H
#define SUI_COMPONENTS_STAT_CARD_H

#include <stdint.h>
#include "lvgl.h"

#define SUI_STAT_COUNT 3

lv_obj_t *sui_stat_card_create(lv_obj_t *parent, const char *title, const char *const keys[SUI_STAT_COUNT],
                               uint32_t color);
void sui_stat_card_set(lv_obj_t *card, const char *value, const char *unit, int32_t percent,
                       const char *const values[SUI_STAT_COUNT]);

#endif
