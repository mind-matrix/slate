#include "settings.h"

#include <stdio.h>
#include <time.h>
#include "about.h"
#include "apps_screen.h"
#include "voice_assistant.h"
#include "bluetooth.h"
#include "components/header.h"
#include "components/list.h"
#include "datetime.h"
#include "display_settings.h"
#include "sound.h"
#include "theme/theme.h"
#include "timezone.h"
#include "wifi.h"

#define SUMMARY 96

typedef struct {
    sui_row_t row;
    const sui_screen_def_t *target;
    void (*summary)(char *out, size_t len);
} entry_t;

static void datetime_summary(char *out, size_t len)
{
    char id[TIMEZONE_ID_MAX], city[TIMEZONE_ID_MAX], clock[8];
    time_t now = time(NULL);
    struct tm local;

    localtime_r(&now, &local);
    strftime(clock, sizeof(clock), "%H:%M", &local);
    timezone_current(id, sizeof(id));
    timezone_city(id, city, sizeof(city));
    snprintf(out, len, "%s · %s", city, clock);
}

static const entry_t entries[] = {
    { { SUI_SYMBOL(LV_SYMBOL_WIFI), SUI_COLOR_CAT_WIFI, "Wi-Fi", "", SUI_TRAILING_CHEVRON }, &wifi_screen, wifi_summary },
    { { SUI_SYMBOL(LV_SYMBOL_BLUETOOTH), SUI_COLOR_CAT_BLUETOOTH, "Bluetooth", "", SUI_TRAILING_CHEVRON },
      &bluetooth_screen, bluetooth_summary },
    { { SUI_VECTOR(SUI_ICON_MONITOR), SUI_COLOR_CAT_DISPLAY, "Display", "", SUI_TRAILING_CHEVRON }, &display_screen,
      display_summary },
    { { SUI_SYMBOL(LV_SYMBOL_VOLUME_MAX), SUI_COLOR_CAT_SOUND, "Sound", "", SUI_TRAILING_CHEVRON }, &sound_screen,
      sound_summary },
    { { SUI_VECTOR(SUI_ICON_CLOCK), SUI_COLOR_CAT_SYSTEM, "Date & time", "", SUI_TRAILING_CHEVRON }, &datetime_screen,
      datetime_summary },
    { { SUI_SYMBOL(LV_SYMBOL_LIST), SUI_COLOR_CAT_WIFI, "Apps", "", SUI_TRAILING_CHEVRON }, &apps_screen, apps_summary },
    { { SUI_VECTOR(SUI_ICON_MIC), SUI_COLOR_CAT_BLUETOOTH, "Voice assistant", "", SUI_TRAILING_CHEVRON },
      &voice_assistant_screen, voice_assistant_summary },
    { { SUI_VECTOR(SUI_ICON_INFO), SUI_COLOR_CAT_SYSTEM, "About", "Build, device, diagnostics", SUI_TRAILING_CHEVRON },
      &about_screen, NULL },
};

#define ENTRY_COUNT (sizeof(entries) / sizeof(entries[0]))

static lv_obj_t *rows[ENTRY_COUNT];

static void open_entry(lv_event_t *e)
{
    const entry_t *entry = lv_event_get_user_data(e);
    sui_nav_push(entry->target, NULL);
}

static void create(lv_obj_t *screen, void *args)
{
    (void)args;
    lv_obj_t *page = sui_page_create(screen, "Settings");
    lv_obj_add_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(page, LV_DIR_VER);
    lv_obj_t *list = sui_list_create(page, SUI_CONTENT_W);

    for (size_t i = 0; i < ENTRY_COUNT; i++) {
        rows[i] = sui_list_row_create(list, &entries[i].row);
        lv_obj_add_event_cb(rows[i], open_entry, LV_EVENT_CLICKED, (void *)&entries[i]);
    }
}

static void on_show(lv_obj_t *screen)
{
    (void)screen;
    char text[SUMMARY];
    for (size_t i = 0; i < ENTRY_COUNT; i++) {
        if (!entries[i].summary)
            continue;
        entries[i].summary(text, sizeof(text));
        sui_list_row_set_detail(rows[i], text);
    }
}

const sui_screen_def_t settings_screen = {
    .name = "settings",
    .create = create,
    .on_show = on_show,
};
