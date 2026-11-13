#include "geometry.h"

/* These match how LVGL rotated touch for the same SLATE_ROTATION before the shell became a
 * compositor, so the panel and touchscreen keep their verified orientation. */
void geometry_touch(int rotation, double nx, double ny, int width, int height, double *x, double *y)
{
    switch (rotation) {
    case 0:
        *x = nx * width;
        *y = ny * height;
        break;
    case 90:
        *x = (1.0 - ny) * width;
        *y = nx * height;
        break;
    case 180:
        *x = (1.0 - nx) * width;
        *y = (1.0 - ny) * height;
        break;
    default:
        *x = ny * width;
        *y = (1.0 - nx) * height;
        break;
    }
}

int geometry_transform(int rotation)
{
    switch (rotation) {
    case 0:
        return 0;
    case 90:
        return 1;
    case 180:
        return 2;
    default:
        return 3;
    }
}
