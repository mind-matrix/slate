#ifndef SLATE_SETTINGS_UI_H
#define SLATE_SETTINGS_UI_H

#include <stdbool.h>
#include <stdint.h>
#include "components/icon.h"
#include "lvgl.h"

lv_obj_t *settings_page_create(lv_obj_t *screen, const char *title);
lv_obj_t *settings_card_create(lv_obj_t *parent);
lv_obj_t *settings_switch_card_create(lv_obj_t *parent, const char *title, const char *detail, uint32_t color);
lv_obj_t *settings_section_create(lv_obj_t *parent, const char *title, lv_obj_t **status);
lv_obj_t *settings_empty_state_create(lv_obj_t *parent, sui_glyph_t glyph, const char *title, const char *detail);
void settings_set_visible(lv_obj_t *obj, bool visible);
lv_obj_t *settings_info_dialog(const char *title, const char *body);

#endif
