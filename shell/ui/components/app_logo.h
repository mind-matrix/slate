#ifndef SUI_COMPONENTS_APP_LOGO_H
#define SUI_COMPONENTS_APP_LOGO_H

#include <stdint.h>
#include "lvgl.h"

#define SUI_APP_LOGO_SOURCE 256

lv_obj_t *sui_app_logo_create(lv_obj_t *parent, const char *path, const char *name, int32_t size);
lv_obj_t *sui_app_icon_logo_create(lv_obj_t *parent, const char *path, const char *name);

#endif
