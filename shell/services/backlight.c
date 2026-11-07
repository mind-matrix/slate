#include "backlight.h"

int backlight_percent_to_level(int percent, int max)
{
    if (percent < BACKLIGHT_MIN_PERCENT)
        percent = BACKLIGHT_MIN_PERCENT;
    if (percent > 100)
        percent = 100;
    int level = (percent * max + 50) / 100;
    return level < 1 ? 1 : level;
}

int backlight_level_to_percent(int level, int max)
{
    if (max <= 0)
        return 0;
    return (level * 100 + max / 2) / max;
}
