#ifndef SUI_COMPONENTS_BUTTON_H
#define SUI_COMPONENTS_BUTTON_H

#include <stdint.h>
#include "lvgl.h"

lv_obj_t *sui_button_create(lv_obj_t *parent, const char *text, uint32_t fill, uint32_t ink);
lv_obj_t *sui_button_secondary_create(lv_obj_t *parent, const char *text);
void sui_button_set_text(lv_obj_t *button, const char *text);

#endif
