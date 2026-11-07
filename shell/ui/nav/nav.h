#ifndef SUI_NAV_H
#define SUI_NAV_H

#include <stddef.h>
#include "lvgl.h"

#define SUI_NAV_MAX 8

typedef struct sui_screen_def {
    const char *name;
    void (*create)(lv_obj_t *screen, void *args);
    void (*on_show)(lv_obj_t *screen);
    void (*on_hide)(lv_obj_t *screen);
} sui_screen_def_t;

void sui_nav_init(const sui_screen_def_t *home, void *args);
void sui_nav_push(const sui_screen_def_t *def, void *args);
void sui_nav_pop(void);
void sui_nav_home(void);
size_t sui_nav_depth(void);
const sui_screen_def_t *sui_nav_top(void);

#endif
