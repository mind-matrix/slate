#include "desktop.h"

#include <sys/stat.h>
#include "apps.h"
#include "apps_screen.h"
#include "components/app_logo.h"
#include "about.h"
#include "clock_app.h"
#include "terminal.h"
#include "components/app_icon.h"
#include "components/wallpaper.h"
#include "platform/platform.h"
#include "theme/theme.h"

#define GRID_PAD_HOR 64
#define GRID_PAD_VER 56
#define GRID_GAP_COL 40
#define GRID_GAP_ROW 32
#define WATCH_MS 2000

typedef struct {
    sui_glyph_t glyph;
    const char *label;
    uint32_t color;
    const sui_screen_def_t *screen;
} builtin_t;

static const builtin_t builtins[] = {
    { SUI_VECTOR(SUI_ICON_TERMINAL), "Terminal", SUI_COLOR_CAT_TERMINAL, &terminal_screen },
    { SUI_VECTOR(SUI_ICON_CLOCK), "Clock", SUI_COLOR_CAT_CLOCK, &clock_app_screen },
    { SUI_VECTOR(SUI_ICON_INFO), "About", SUI_COLOR_CAT_SYSTEM, &about_screen },
};

static struct {
    lv_obj_t *grid;
    app_t installed[APPS_MAX];
    size_t count;
    int64_t changed_at;
    lv_timer_t *timer;
} view;

static void open_app(lv_event_t *e)
{
    const builtin_t *builtin = lv_event_get_user_data(e);
    sui_nav_push(builtin->screen, (void *)builtin->label);
}

static void open_installed(lv_event_t *e)
{
    const app_t *app = lv_event_get_user_data(e);
    apps_open(app->id);
}

static void show_tiles(void)
{
    lv_obj_clean(view.grid);
    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
        lv_obj_t *icon = sui_app_icon_create(view.grid, builtins[i].glyph, builtins[i].label, builtins[i].color);
        lv_obj_add_event_cb(icon, open_app, LV_EVENT_CLICKED, (void *)&builtins[i]);
    }
    view.count = apps_list(view.installed, APPS_MAX);
    for (size_t i = 0; i < view.count; i++) {
        const app_t *app = &view.installed[i];
        if (!apps_has(app->granted, "gui"))
            continue;
        lv_obj_t *icon = sui_app_icon_logo_create(view.grid, app->logo, app->name);
        lv_obj_add_event_cb(icon, open_installed, LV_EVENT_CLICKED, (void *)app);
    }
}

static int64_t apps_changed_at(void)
{
    struct stat info;
    return stat(APPS_DIR, &info) == 0 ? (int64_t)info.st_mtime : 0;
}

static void watch(lv_timer_t *timer)
{
    (void)timer;
    int64_t changed = apps_changed_at();
    if (changed != view.changed_at) {
        view.changed_at = changed;
        show_tiles();
    }
}

static void create(lv_obj_t *screen, void *args)
{
    (void)args;
    sui_wallpaper_create(screen);

    lv_obj_t *grid = sui_box_create(screen);
    lv_obj_set_size(grid, SUI_SCREEN_W, SUI_SCREEN_H - SUI_STATUS_BAR_H);
    lv_obj_set_pos(grid, 0, SUI_STATUS_BAR_H);
    lv_obj_set_style_pad_hor(grid, GRID_PAD_HOR, 0);
    lv_obj_set_style_pad_ver(grid, GRID_PAD_VER, 0);
    lv_obj_set_style_pad_column(grid, GRID_GAP_COL, 0);
    lv_obj_set_style_pad_row(grid, GRID_GAP_ROW, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);

    view.grid = grid;
    show_tiles();
}

static void on_show(lv_obj_t *screen)
{
    (void)screen;
    view.changed_at = apps_changed_at();
    view.timer = lv_timer_create(watch, WATCH_MS, NULL);
}

static void on_hide(lv_obj_t *screen)
{
    (void)screen;
    lv_timer_delete(view.timer);
    view.timer = NULL;
}

const sui_screen_def_t desktop_screen = {
    .name = "desktop",
    .create = create,
    .on_show = on_show,
    .on_hide = on_hide,
};
