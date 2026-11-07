#include "platform/backend.h"

lv_display_t *sui_backend_open(void)
{
    lv_display_t *disp = lv_sdl_window_create(SUI_SCREEN_W, SUI_SCREEN_H);
    lv_sdl_window_set_title(disp, "Slate");
    lv_sdl_mouse_create();
    return disp;
}
