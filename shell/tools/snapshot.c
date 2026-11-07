#include <stdio.h>
#include <string.h>

#include "about.h"
#include "bluetooth.h"
#include "components/status_bar.h"
#include "datetime.h"
#include "desktop.h"
#include "diagnostics.h"
#include "display_settings.h"
#include "idle.h"
#include "platform/platform.h"
#include "power_screen.h"
#include "quick_drawer.h"
#include "settings.h"
#include "sound.h"
#include "terminal.h"
#include "theme/theme.h"
#include "timezone_picker.h"
#include "wifi.h"
#include "wifi_join.h"

#define SETTLE_MS 1500

static const sui_screen_def_t *const screens[] = {
    &desktop_screen,  &settings_screen, &about_screen, &diagnostics_screen, &datetime_screen,
    &timezone_picker_screen, &wifi_screen, &bluetooth_screen, &display_screen, &sound_screen,
    &power_screen, &terminal_screen,
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

#define TYPED_COMMAND "ls --color=always / && printf '\\033[1;33mslate\\033[0m \\u2500\\u2500 ok\\n'"

static void type_command(void)
{
    sui_run_for(SETTLE_MS);
    for (const char *c = TYPED_COMMAND; *c; c++)
        sui_key_dispatch((uint32_t)(unsigned char)*c, 0);
    sui_key_dispatch(SUI_KEY_ENTER, 0);
}

static void open_overlay(const char *name)
{
    if (strcmp(name, "terminal") == 0)
        type_command();
    if (strcmp(name, "drawer") == 0)
        quick_drawer_open();
    if (strcmp(name, "wifi_join") == 0) {
        net_ap_t ap = { .ssid = "Home-2.4G", .frequency = 2437, .security = NET_SECURITY_WPA2 };
        wifi_join_open(&ap, NULL);
    }
}

int main(int argc, char **argv)
{
    const char *name = argc == 3 ? argv[2] : "";
    bool drawer = strcmp(name, "drawer") == 0;
    bool join = strcmp(name, "wifi_join") == 0;
    const sui_screen_def_t *screen = drawer ? &desktop_screen : join ? &wifi_screen : find_screen(name);
    if (!screen) {
        fprintf(stderr, "usage: %s <out.rgb565> <screen name>\n", argv[0]);
        return 2;
    }

    sui_headless_init();
    sui_theme_init();
    idle_start();
    sui_nav_init(&desktop_screen, NULL);
    if (screen != &desktop_screen)
        sui_nav_push(screen, NULL);
    sui_status_bar_create(lv_layer_top(), quick_drawer_open);
    open_overlay(name);

    sui_run_for(SETTLE_MS);
    lv_refr_now(NULL);
    return write_frame(argv[1]);
}
