#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "about.h"
#include "apps.h"
#include "apps_screen.h"
#include "voice_assistant.h"
#include "bluetooth.h"
#include "clock.h"
#include "clock_alerts.h"
#include "clock_app.h"
#include "clock_sheets.h"
#include "notify.h"
#include "components/status_bar.h"
#include "datetime.h"
#include "desktop.h"
#include "diagnostics.h"
#include "display_settings.h"
#include "idle.h"
#include "platform/platform.h"
#include "prefs.h"
#include "power_screen.h"
#include "quick_drawer.h"
#include "settings.h"
#include "software_update.h"
#include "sound.h"
#include "terminal.h"
#include "theme/theme.h"
#include "timezone_picker.h"
#include "wifi.h"
#include "wallpaper_screen.h"
#include "wifi_join.h"

#define SETTLE_MS 1500

static const sui_screen_def_t *const screens[] = {
    &desktop_screen,  &settings_screen, &about_screen, &diagnostics_screen, &datetime_screen,
    &timezone_picker_screen, &wifi_screen, &bluetooth_screen, &display_screen, &sound_screen,
    &power_screen, &terminal_screen, &software_update_screen, &clock_app_screen, &apps_screen, &voice_assistant_screen,
    &wallpaper_screen,
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

static void seed_home(void)
{
    static char home[] = "/tmp/slate-snapshot-XXXXXX";
    if (mkdtemp(home))
        setenv("HOME", home, 1);
}

static void seed_notifications(void)
{
    notify_post(NOTIFY_BLUETOOTH, NOTIFY_ICON_BLUETOOTH, "connected:/org/bluez/hci0/dev_PICO", "Pico connected",
                "Speaker");
    notify_post(NOTIFY_CLOCK, NOTIFY_ICON_ALARM, "alarm:9", "Missed alarm", "07:00 · Wake up");
    notify_post(NOTIFY_UPDATE, NOTIFY_ICON_UPDATE, "available", "Update available", "Slate 2026.11.8 is ready to install");
    notify_post(NOTIFY_CLOCK, NOTIFY_ICON_REMINDER, "reminder:8", "Water the plants", "Reminder for 12:30");
    notify_post(NOTIFY_CLOCK, NOTIFY_ICON_TIMER, "timer:7", "Timer done", "Tea · 4:00 timer");
}

static void add_alarm(int hour, int minute, uint8_t days, bool on, const char *label)
{
    clock_alarm_t *a = clock_add_alarm();
    a->hour = (uint8_t)hour;
    a->minute = (uint8_t)minute;
    a->days = days;
    a->on = on;
    snprintf(a->label, sizeof(a->label), "%s", label);
}

static void add_reminder(int64_t offset, bool done, const char *text)
{
    clock_reminder_t *r = clock_add_reminder();
    r->when = (int64_t)time(NULL) + offset;
    r->done = done;
    r->fired = offset <= 0;
    snprintf(r->text, sizeof(r->text), "%s", text);
}

static void seed_clock(void)
{
    add_alarm(7, 0, CLOCK_WEEKDAYS, true, "Wake up");
    add_alarm(8, 30, CLOCK_WEEKEND, false, "Weekend");
    add_alarm(13, 15, CLOCK_WEEKDAYS, true, "Lunch break");
    add_alarm(22, 45, CLOCK_EVERY_DAY, true, "Wind down");
    add_alarm(6, 10, 0, false, "Airport");
    clock_timer_t *laundry = clock_add_timer(2700000, "Laundry");
    clock_timer_pause(laundry, clock_mono_ms() + 716000);
    clock_add_timer(600000 - 169000, "Pasta")->total_ms = 600000;
    add_reminder(4 * 3600, false, "Call the electrician");
    add_reminder(7 * 3600, false, "Take out the recycling");
    add_reminder(26 * 3600, false, "Renew bike insurance");
    add_reminder(6 * 86400, false, "Birthday dinner");
    add_reminder(-600, true, "Water the plants");
    clock_stopwatch_t *sw = clock_stopwatch();
    sw->base_ms = 83450;
    sw->lap_count = 3;
    sw->laps[0] = 24810;
    sw->laps[1] = 52370;
    sw->laps[2] = 77020;
}

static void seed_alert(const char *name)
{
    clock_state_t *s = clock_state();
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    if (strcmp(name, "alarm_ring") == 0) {
        add_alarm(local.tm_hour, local.tm_min, CLOCK_EVERY_DAY, true, "Wake up");
        s->last_seen = (int64_t)now - 60;
    } else if (strcmp(name, "timer_done") == 0) {
        clock_add_timer(300, "Pasta")->total_ms = 600000;
    } else if (strcmp(name, "reminder_banner") == 0) {
        add_reminder(-5, false, "Call the electrician");
        s->reminders[s->reminder_count - 1].fired = false;
    }
}

static const char *const clock_tabs[] = { "clock", "clock_timers", "clock_stopwatch", "clock_reminders" };

static bool is_clock_tab(const char *name, size_t *tab)
{
    for (size_t i = 0; i < sizeof(clock_tabs) / sizeof(clock_tabs[0]); i++)
        if (strcmp(clock_tabs[i], name) == 0) {
            *tab = i;
            return true;
        }
    return false;
}

static bool is_alert(const char *name)
{
    return strcmp(name, "alarm_ring") == 0 || strcmp(name, "timer_done") == 0 || strcmp(name, "reminder_banner") == 0;
}

static void open_overlay(const char *name)
{
    if (strcmp(name, "terminal") == 0)
        type_command();
    if (strcmp(name, "drawer") == 0 || strcmp(name, "drawer_empty") == 0)
        quick_drawer_open();
    if (strcmp(name, "alarm_edit") == 0)
        clock_sheet_alarm_open(1, NULL);
    if (strcmp(name, "reminder_edit") == 0)
        clock_sheet_reminder_open(8, NULL);
    if (strcmp(name, "wifi_join") == 0) {
        net_ap_t ap = { .ssid = "Home-2.4G", .frequency = 2437, .security = NET_SECURITY_WPA2 };
        wifi_join_open(&ap, NULL);
    }
}

int main(int argc, char **argv)
{
    const char *name = argc == 3 ? argv[2] : "";
    bool drawer = strcmp(name, "drawer") == 0 || strcmp(name, "drawer_empty") == 0 || is_alert(name);
    bool join = strcmp(name, "wifi_join") == 0;
    bool sheet = strcmp(name, "alarm_edit") == 0 || strcmp(name, "reminder_edit") == 0;
    size_t tab = 0;
    bool clock = is_clock_tab(name, &tab) || sheet;
    bool detail = strcmp(name, "app_detail") == 0;
    bool app_bar = strcmp(name, "app_bar") == 0;
    const char *paper = strncmp(name, "desktop_", 8) == 0 ? name + 8 : NULL;
    const sui_screen_def_t *screen = drawer || app_bar || paper ? &desktop_screen
                                     : detail ? &app_detail_screen
                                     : join   ? &wifi_screen
                                     : clock ? &clock_app_screen
                                             : find_screen(name);
    if (!screen) {
        fprintf(stderr, "usage: %s <out.rgb565> <screen name>\n", argv[0]);
        return 2;
    }

    seed_home();
    if (paper)
        prefs_set_text("wallpaper", paper);
    sui_headless_init();
    sui_theme_init();
    idle_start();
    clock_alerts_start();
    if (strcmp(name, "drawer") == 0)
        seed_notifications();
    if (clock)
        seed_clock();
    seed_alert(name);
    clock_app_select(strcmp(name, "reminder_edit") == 0 ? 3 : tab);
    sui_nav_init(&desktop_screen, NULL);
    static app_t apps[APPS_MAX];
    void *args = NULL;
    if (detail) {
        if (!apps_list(apps, APPS_MAX))
            return 1;
        args = &apps[0];
    }
    if (screen != &desktop_screen)
        sui_nav_push(screen, args);
    sui_status_bar_create(lv_layer_top(), quick_drawer_open);
    open_overlay(name);
    if (strcmp(name, "app_bar") == 0) {
        sui_status_bar_set_app(true, NULL, NULL);
        sui_status_bar_set_mic("Marvin", false, NULL);
    }

    sui_run_for(SETTLE_MS);
    lv_refr_now(NULL);
    return write_frame(argv[1]);
}
