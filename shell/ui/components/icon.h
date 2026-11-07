#ifndef SUI_COMPONENTS_ICON_H
#define SUI_COMPONENTS_ICON_H

#include <stdint.h>
#include "lvgl.h"

typedef enum {
    SUI_ICON_NONE,
    SUI_ICON_CLOCK,
    SUI_ICON_INFO,
    SUI_ICON_TERMINAL,
    SUI_ICON_PULSE,
    SUI_ICON_MONITOR,
    SUI_ICON_SUN,
    SUI_ICON_LOCK,
    SUI_ICON_MIC,
    SUI_ICON_ALARM,
    SUI_ICON_TIMER,
    SUI_ICON_REMINDER,
} sui_icon_t;

typedef struct {
    const char *symbol;
    sui_icon_t icon;
} sui_glyph_t;

#define SUI_SYMBOL(s) ((sui_glyph_t){ .symbol = (s) })
#define SUI_VECTOR(i) ((sui_glyph_t){ .icon = (i) })

lv_obj_t *sui_glyph_create(lv_obj_t *parent, sui_glyph_t glyph, int32_t size, uint32_t color);

#endif
