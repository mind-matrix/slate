#ifndef SLATE_COMPOSITOR_GEOMETRY_H
#define SLATE_COMPOSITOR_GEOMETRY_H

/* Maps a touch point, reported from 0 to 1 along the panel's own axes, to the landscape layout
 * the shell draws in. rotation is the output rotation in degrees (0, 90, 180 or 270), the same
 * value SLATE_ROTATION sets. */
void geometry_touch(int rotation, double nx, double ny, int width, int height, double *x, double *y);

/* The wl_output transform for a rotation in degrees. Unknown values mean 270, the default. */
int geometry_transform(int rotation);

#endif
