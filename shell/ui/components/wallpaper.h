#ifndef SUI_COMPONENTS_WALLPAPER_H
#define SUI_COMPONENTS_WALLPAPER_H

#include "lvgl.h"

#define SUI_WALLPAPER_COUNT 8
#define SUI_WALLPAPER_DEFAULT 0

const char *sui_wallpaper_name(int id);
const char *sui_wallpaper_key(int id);
int sui_wallpaper_find(const char *key);

/* Full-screen wallpaper with the SLATE wordmark. */
lv_obj_t *sui_wallpaper_create(lv_obj_t *parent, int id);
/* Fixed-size wallpaper for previews and thumbnails; drawn by the same code, scaled to the size. */
lv_obj_t *sui_wallpaper_swatch_create(lv_obj_t *parent, int id, int32_t width, int32_t height);
void sui_wallpaper_set(lv_obj_t *wallpaper, int id);

#endif
