#ifndef SUI_COMPONENTS_DIALOG_H
#define SUI_COMPONENTS_DIALOG_H

#include "lvgl.h"

lv_obj_t *sui_dialog_create(const char *title, const char *body);
lv_obj_t *sui_dialog_actions(lv_obj_t *dialog);
void sui_dialog_close(lv_obj_t *dialog);

#endif
