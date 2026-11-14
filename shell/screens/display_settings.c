#include "display_settings.h"

#include <stdio.h>
#include "backlight.h"
#include "components/list.h"
#include "components/segmented.h"
#include "components/slider_row.h"
#include "idle.h"
#include "platform/platform.h"
#include "settings_ui.h"
#include "theme/theme.h"
#include "wallpaper_screen.h"
#include "components/wallpaper.h"

#define TEXT 64

typedef struct {
    lv_obj_t *brightness;
    lv_obj_t *timeout;
    lv_obj_t *wallpaper;
} display_view_t;

static display_view_t view;

void display_summary(char *out, size_t len)
{
    int percent;
    const char *off = idle_labels[idle_option_index(idle_timeout())];
    if (!backlight_get(&percent))
        snprintf(out, len, "Screen off after %s", off);
    else if (idle_timeout() > 0)
        snprintf(out, len, "Brightness %d%% · off after %s", percent, off);
    else
        snprintf(out, len, "Brightness %d%% · always on", percent);
}

static void set_brightness(lv_event_t *e)
{
    backlight_set((int)lv_slider_get_value(lv_event_get_target(e)));
}

static void set_timeout(lv_event_t *e)
{
    (void)e;
    idle_set_timeout(idle_options[sui_segmented_get(view.timeout)]);
}

static void refresh(void)
{
    int percent;
    bool available = backlight_get(&percent);
    if (available)
        sui_slider_row_set_value(view.brightness, percent);
    sui_slider_row_set_available(view.brightness, available);
    sui_segmented_set(view.timeout, idle_option_index(idle_timeout()));
    sui_list_row_set_detail(view.wallpaper, sui_wallpaper_name(wallpaper_current()));
}

static void add_info_row(lv_obj_t *list, const char *title, const char *value)
{
    sui_row_t spec = { .title = title, .detail = value, .trailing = SUI_TRAILING_VALUE };
    sui_list_row_create(list, &spec);
}

static void open_wallpaper(lv_event_t *e)
{
    (void)e;
    sui_nav_push(&wallpaper_screen, NULL);
}

static void create(lv_obj_t *screen, void *args)
{
    (void)args;
    char panel[TEXT];
    lv_obj_t *page = settings_page_create(screen, "Display");

    lv_obj_t *section = settings_section_create(page, "BRIGHTNESS", NULL);
    view.brightness = sui_slider_row_create(section, SUI_VECTOR(SUI_ICON_SUN), SUI_COLOR_CAT_DISPLAY,
                                            BACKLIGHT_MIN_PERCENT, 100);
    lv_obj_add_event_cb(sui_slider_row_slider(view.brightness), set_brightness, LV_EVENT_VALUE_CHANGED, NULL);

    section = settings_section_create(page, "SCREEN OFF AFTER", NULL);
    view.timeout = sui_segmented_create(section, idle_labels, IDLE_OPTION_COUNT, SUI_COLOR_CAT_DISPLAY);
    lv_obj_add_event_cb(view.timeout, set_timeout, LV_EVENT_VALUE_CHANGED, NULL);

    section = settings_section_create(page, "WALLPAPER", NULL);
    lv_obj_t *picker = sui_list_create(section, LV_PCT(100));
    sui_row_t wallpaper = { .title = "Wallpaper", .detail = "", .trailing = SUI_TRAILING_CHEVRON };
    view.wallpaper = sui_list_row_create(picker, &wallpaper);
    lv_obj_add_event_cb(view.wallpaper, open_wallpaper, LV_EVENT_CLICKED, NULL);

    section = settings_section_create(page, "SCREEN", NULL);
    lv_obj_t *list = sui_list_create(section, LV_PCT(100));
    add_info_row(list, "Orientation", "Landscape");
    snprintf(panel, sizeof(panel), "Touch Display 2 · %d × %d", SUI_SCREEN_W, SUI_SCREEN_H);
    add_info_row(list, "Panel", panel);
    refresh();
}

static void on_show(lv_obj_t *screen)
{
    (void)screen;
    refresh();
}

const sui_screen_def_t display_screen = {
    .name = "display",
    .create = create,
    .on_show = on_show,
};
