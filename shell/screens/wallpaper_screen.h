#ifndef SLATE_WALLPAPER_SCREEN_H
#define SLATE_WALLPAPER_SCREEN_H

#include "nav/nav.h"

extern const sui_screen_def_t wallpaper_screen;

int wallpaper_current(void);
void wallpaper_select(int id);

#endif
