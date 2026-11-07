#ifndef SUI_COMPONENTS_LIST_H
#define SUI_COMPONENTS_LIST_H

#include "components/icon.h"

typedef enum {
    SUI_TRAILING_NONE,
    SUI_TRAILING_CHEVRON,
    SUI_TRAILING_VALUE,
    SUI_TRAILING_RADIO,
} sui_trailing_t;

typedef struct {
    sui_glyph_t glyph;
    uint32_t color;
    const char *title;
    const char *detail;
    sui_trailing_t trailing;
} sui_row_t;

lv_obj_t *sui_list_create(lv_obj_t *parent, int32_t width);
lv_obj_t *sui_list_row_create(lv_obj_t *list, const sui_row_t *row);
void sui_list_row_set_detail(lv_obj_t *row, const char *text);
void sui_list_row_set_detail_color(lv_obj_t *row, uint32_t color);
void sui_list_row_set_checked(lv_obj_t *row, bool checked);
lv_obj_t *sui_section_label_create(lv_obj_t *parent, const char *text);

#endif
