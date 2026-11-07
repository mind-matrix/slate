#ifndef SLATE_BACKLIGHT_H
#define SLATE_BACKLIGHT_H

#include <stdbool.h>

#define BACKLIGHT_MIN_PERCENT 5

int backlight_percent_to_level(int percent, int max);
int backlight_level_to_percent(int level, int max);
bool backlight_get(int *percent);
bool backlight_set(int percent);
bool backlight_blank(void);

#endif
