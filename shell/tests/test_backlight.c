#include "backlight.h"
#include "check.h"

static void test_percent_to_level(void)
{
    CHECK(backlight_percent_to_level(100, 31) == 31);
    CHECK(backlight_percent_to_level(50, 31) == 16);
    CHECK(backlight_percent_to_level(0, 31) == 2);
    CHECK(backlight_percent_to_level(150, 31) == 31);
    CHECK(backlight_percent_to_level(5, 10) == 1);
}

static void test_level_to_percent(void)
{
    CHECK(backlight_level_to_percent(31, 31) == 100);
    CHECK(backlight_level_to_percent(15, 31) == 48);
    CHECK(backlight_level_to_percent(0, 31) == 0);
    CHECK(backlight_level_to_percent(5, 0) == 0);
}

int main(void)
{
    test_percent_to_level();
    test_level_to_percent();
    return failures ? 1 : 0;
}
