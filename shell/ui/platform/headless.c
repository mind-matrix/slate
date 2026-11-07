#include "platform/backend.h"

static uint16_t pixels[SUI_SCREEN_W * SUI_SCREEN_H];

static void flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    (void)area;
    (void)px_map;
    lv_display_flush_ready(disp);
}

lv_display_t *sui_headless_open(void)
{
    lv_display_t *disp = lv_display_create(SUI_SCREEN_W, SUI_SCREEN_H);
    lv_display_set_flush_cb(disp, flush);
    lv_display_set_buffers(disp, pixels, NULL, sizeof(pixels), LV_DISPLAY_RENDER_MODE_DIRECT);
    return disp;
}

const uint16_t *sui_headless_pixels(void)
{
    return pixels;
}
