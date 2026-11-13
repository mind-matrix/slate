#ifndef SLATE_APPS_SCREEN_H
#define SLATE_APPS_SCREEN_H

#include <stdbool.h>
#include <stddef.h>
#include "nav/nav.h"

extern const sui_screen_def_t apps_screen;
extern const sui_screen_def_t app_detail_screen;

void apps_summary(char *out, size_t len);
lv_obj_t *apps_badge_create(lv_obj_t *parent, bool dev);

#endif
