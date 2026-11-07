#include "backlight.h"

static int brightness = 80;

bool backlight_get(int *percent)
{
    *percent = brightness;
    return true;
}

bool backlight_set(int percent)
{
    brightness = percent;
    return true;
}

bool backlight_blank(void)
{
    return true;
}
