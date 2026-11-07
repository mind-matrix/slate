#include "platform/backend.h"

#include <drm_fourcc.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#define DRAW_LINES 80
#define BYTES_PER_PIXEL 2

typedef struct {
    int fd;
    uint32_t fb_id;
    uint8_t *map;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
} output_t;

static output_t out;
static uint16_t draw_buf[SUI_SCREEN_W * DRAW_LINES];
static uint16_t rotated_buf[SUI_SCREEN_W * DRAW_LINES];

static drmModeConnector *find_dsi_connector(int fd, const drmModeRes *res)
{
    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
        if (conn && conn->connection == DRM_MODE_CONNECTED &&
            conn->connector_type == DRM_MODE_CONNECTOR_DSI && conn->count_modes > 0)
            return conn;
        drmModeFreeConnector(conn);
    }
    return NULL;
}

static uint32_t find_crtc(int fd, const drmModeRes *res, const drmModeConnector *conn)
{
    for (int i = 0; i < conn->count_encoders; i++) {
        drmModeEncoder *enc = drmModeGetEncoder(fd, conn->encoders[i]);
        if (!enc)
            continue;
        uint32_t possible = enc->possible_crtcs;
        drmModeFreeEncoder(enc);
        for (int j = 0; j < res->count_crtcs; j++)
            if (possible & (1u << j))
                return res->crtcs[j];
    }
    return 0;
}

static bool create_framebuffer(int fd, const drmModeModeInfo *mode)
{
    struct drm_mode_create_dumb create = {
        .width = mode->hdisplay,
        .height = mode->vdisplay,
        .bpp = BYTES_PER_PIXEL * 8,
    };
    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0)
        return false;

    uint32_t handles[4] = { create.handle };
    uint32_t pitches[4] = { create.pitch };
    uint32_t offsets[4] = { 0 };
    if (drmModeAddFB2(fd, create.width, create.height, DRM_FORMAT_RGB565,
                      handles, pitches, offsets, &out.fb_id, 0) < 0)
        return false;

    struct drm_mode_map_dumb map = { .handle = create.handle };
    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0)
        return false;
    out.map = mmap(NULL, create.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)map.offset);
    if (out.map == MAP_FAILED)
        return false;

    memset(out.map, 0, create.size);
    out.fd = fd;
    out.pitch = create.pitch;
    out.width = create.width;
    out.height = create.height;
    return true;
}

static bool open_output(const char *path)
{
    int fd = open(path, O_RDWR | O_CLOEXEC);
    if (fd < 0)
        return false;

    drmModeRes *res = drmModeGetResources(fd);
    drmModeConnector *conn = res ? find_dsi_connector(fd, res) : NULL;
    uint32_t crtc = conn ? find_crtc(fd, res, conn) : 0;
    bool ok = crtc && create_framebuffer(fd, &conn->modes[0]) &&
              drmModeSetCrtc(fd, crtc, out.fb_id, 0, 0, &conn->connector_id, 1, &conn->modes[0]) == 0;

    drmModeFreeConnector(conn);
    drmModeFreeResources(res);
    if (!ok)
        close(fd);
    return ok;
}

static bool open_any_output(void)
{
    const char *path = getenv("SLATE_DRM_DEVICE");
    if (path)
        return open_output(path);

    char candidate[32];
    for (int i = 0; i < 4; i++) {
        snprintf(candidate, sizeof(candidate), "/dev/dri/card%d", i);
        if (open_output(candidate))
            return true;
    }
    return false;
}

static lv_display_rotation_t rotation_from_env(void)
{
    const char *value = getenv("SLATE_ROTATION");
    switch (value ? atoi(value) : 270) {
    case 0:
        return LV_DISPLAY_ROTATION_0;
    case 90:
        return LV_DISPLAY_ROTATION_90;
    case 180:
        return LV_DISPLAY_ROTATION_180;
    default:
        return LV_DISPLAY_ROTATION_270;
    }
}

static void flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    lv_display_rotation_t rotation = lv_display_get_rotation(disp);
    lv_area_t target = *area;
    const uint8_t *src = px_map;
    uint32_t src_stride = lv_draw_buf_width_to_stride(lv_area_get_width(area), LV_COLOR_FORMAT_RGB565);

    if (rotation != LV_DISPLAY_ROTATION_0) {
        lv_display_rotate_area(disp, &target);
        uint32_t rotated_stride = lv_draw_buf_width_to_stride(lv_area_get_width(&target), LV_COLOR_FORMAT_RGB565);
        lv_draw_sw_rotate(px_map, rotated_buf, lv_area_get_width(area), lv_area_get_height(area),
                          (int32_t)src_stride, (int32_t)rotated_stride, rotation, LV_COLOR_FORMAT_RGB565);
        src = (const uint8_t *)rotated_buf;
        src_stride = rotated_stride;
    }

    size_t row_bytes = (size_t)lv_area_get_width(&target) * BYTES_PER_PIXEL;
    for (int32_t y = target.y1; y <= target.y2; y++)
        memcpy(out.map + (size_t)y * out.pitch + (size_t)target.x1 * BYTES_PER_PIXEL,
               src + (size_t)(y - target.y1) * src_stride, row_bytes);

    lv_display_flush_ready(disp);
}

lv_display_t *sui_backend_open(void)
{
    if (!open_any_output()) {
        fprintf(stderr, "slate: no connected DSI display found\n");
        return NULL;
    }

    lv_display_t *disp = lv_display_create((int32_t)out.width, (int32_t)out.height);
    lv_display_set_flush_cb(disp, flush);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_rotation(disp, rotation_from_env());

    if (!sui_touch_open(disp))
        fprintf(stderr, "slate: no touchscreen found\n");
    return disp;
}
