#include <math.h>

#include "check.h"
#include "geometry.h"

#define W 1280
#define H 720

static int near(double a, double b)
{
    return fabs(a - b) < 0.5;
}

static void test_touch_270(void)
{
    double x, y;
    /* The panel is 720 wide and 1280 tall; the shell's top-left sits at the panel's top-right. */
    geometry_touch(270, 1.0, 0.0, W, H, &x, &y);
    CHECK(near(x, 0) && near(y, 0));
    geometry_touch(270, 0.0, 1.0, W, H, &x, &y);
    CHECK(near(x, W) && near(y, H));
    geometry_touch(270, 0.5, 0.25, W, H, &x, &y);
    CHECK(near(x, 320) && near(y, 360));
}

static void test_touch_other(void)
{
    double x, y;
    geometry_touch(0, 0.25, 0.5, W, H, &x, &y);
    CHECK(near(x, 320) && near(y, 360));
    geometry_touch(90, 0.0, 1.0, W, H, &x, &y);
    CHECK(near(x, 0) && near(y, 0));
    geometry_touch(180, 1.0, 1.0, W, H, &x, &y);
    CHECK(near(x, 0) && near(y, 0));
}

static void test_transform(void)
{
    CHECK(geometry_transform(0) == 0);
    CHECK(geometry_transform(90) == 1);
    CHECK(geometry_transform(180) == 2);
    CHECK(geometry_transform(270) == 3);
    CHECK(geometry_transform(45) == 3);
}

int main(void)
{
    test_touch_270();
    test_touch_other();
    test_transform();
    return failures ? 1 : 0;
}
