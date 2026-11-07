#ifndef SUI_COMPONENTS_APP_ICON_H
#define SUI_COMPONENTS_APP_ICON_H

#include <stdint.h>
#include "lvgl.h"

#define SUI_APP_ICON_W 160
#define SUI_APP_ICON_H 168

lv_obj_t *sui_app_icon_create(lv_obj_t *parent, const char *glyph, const char *label, uint32_t color);

#endif
