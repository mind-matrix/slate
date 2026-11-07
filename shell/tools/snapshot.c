#include <stdio.h>
#include <string.h>

#include "about.h"
#include "components/status_bar.h"
#include "desktop.h"
#include "diagnostics.h"
#include "platform/platform.h"
#include "settings.h"
#include "theme/theme.h"

static const sui_screen_def_t *const screens[] = {
    &desktop_screen,
    &settings_screen,
    &about_screen,
    &diagnostics_screen,
};

static const sui_screen_def_t *find_screen(const char *name)
{
    for (size_t i = 0; i < sizeof(screens) / sizeof(screens[0]); i++)
        if (strcmp(screens[i]->name, name) == 0)
            return screens[i];
    return NULL;
}

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
    const sui_screen_def_t *screen = argc == 3 ? find_screen(argv[2]) : NULL;
    if (!screen) {
        fprintf(stderr, "usage: %s <out.rgb565> desktop|settings|about|diagnostics\n", argv[0]);
        return 2;
    }

    sui_headless_init();
    sui_theme_init();
    sui_nav_init(&desktop_screen, NULL);
    if (screen != &desktop_screen)
        sui_nav_push(screen, NULL);
    sui_status_bar_create(lv_layer_top());

    lv_timer_handler();
    lv_refr_now(NULL);
    return write_frame(argv[1]);
}
