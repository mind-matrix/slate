#include "settings.h"

#include "about.h"
#include "app_stub.h"
#include "components/header.h"
#include "components/list.h"
#include "theme/theme.h"

typedef struct {
    sui_row_t row;
    const sui_screen_def_t *target;
} entry_t;

static const entry_t entries[] = {
    { { SUI_SYMBOL(LV_SYMBOL_WIFI), SUI_COLOR_CAT_WIFI, "Wi-Fi", NULL, SUI_TRAILING_CHEVRON }, &app_stub_screen },
    { { SUI_SYMBOL(LV_SYMBOL_BLUETOOTH), SUI_COLOR_CAT_BLUETOOTH, "Bluetooth", NULL, SUI_TRAILING_CHEVRON }, &app_stub_screen },
    { { SUI_VECTOR(SUI_ICON_MONITOR), SUI_COLOR_CAT_DISPLAY, "Display", NULL, SUI_TRAILING_CHEVRON }, &app_stub_screen },
    { { SUI_SYMBOL(LV_SYMBOL_VOLUME_MAX), SUI_COLOR_CAT_SOUND, "Sound", NULL, SUI_TRAILING_CHEVRON }, &app_stub_screen },
    { { SUI_VECTOR(SUI_ICON_CLOCK), SUI_COLOR_CAT_SYSTEM, "Date & time", NULL, SUI_TRAILING_CHEVRON }, &app_stub_screen },
    { { SUI_VECTOR(SUI_ICON_INFO), SUI_COLOR_CAT_SYSTEM, "About", "Build, device, diagnostics", SUI_TRAILING_CHEVRON },
      &about_screen },
};

static void open_entry(lv_event_t *e)
{
    const entry_t *entry = lv_event_get_user_data(e);
    sui_nav_push(entry->target, (void *)entry->row.title);
}

static void create(lv_obj_t *screen, void *args)
{
    (void)args;
    lv_obj_t *page = sui_page_create(screen, "Settings");
    lv_obj_t *list = sui_list_create(page, SUI_CONTENT_W);

    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        lv_obj_t *row = sui_list_row_create(list, &entries[i].row);
        lv_obj_add_event_cb(row, open_entry, LV_EVENT_CLICKED, (void *)&entries[i]);
    }
}

const sui_screen_def_t settings_screen = {
    .name = "settings",
    .create = create,
};
