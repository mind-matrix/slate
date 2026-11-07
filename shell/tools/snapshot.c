#include <stdio.h>
#include <string.h>

#include "app_stub.h"
#include "components/status_bar.h"
#include "desktop.h"
#include "platform/platform.h"
#include "theme/theme.h"

static int write_frame(const char *path)
{
    FILE *file = fopen(path, "wb");
    if (!file)
        return 1;
    size_t count = (size_t)SUI_SCREEN_W * SUI_SCREEN_H;
    size_t written = fwrite(sui_headless_pixels(), sizeof(uint16_t), count, file);
    fclose(file);
    return written == count ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <out.rgb565> [app-title]\n", argv[0]);
        return 2;
    }

    sui_headless_init();
    sui_theme_init();
    sui_nav_init(&desktop_screen, NULL);
    if (argc > 2)
        sui_nav_push(&app_stub_screen, argv[2]);
    sui_status_bar_create(lv_layer_top());

    lv_timer_handler();
    lv_refr_now(NULL);
    return write_frame(argv[1]);
}
