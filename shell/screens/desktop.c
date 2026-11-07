#include "desktop.h"

#include "app_stub.h"
#include "components/app_icon.h"
#include "components/wallpaper.h"
#include "platform/platform.h"
#include "theme/theme.h"

#define GRID_PAD_HOR 64
#define GRID_PAD_VER 56
#define GRID_GAP_COL 40
#define GRID_GAP_ROW 32

typedef struct {
    const char *glyph;
    const char *label;
    uint32_t color;
} app_t;

static const app_t apps[] = {
    { LV_SYMBOL_SETTINGS, "Settings", SUI_COLOR_CAT_WIFI },
    { ">_", "Terminal", SUI_COLOR_CAT_TERMINAL },
    { "i", "About", SUI_COLOR_CAT_SYSTEM },
};

static void open_app(lv_event_t *e)
{
    const app_t *app = lv_event_get_user_data(e);
    sui_nav_push(&app_stub_screen, (void *)app->label);
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

    for (size_t i = 0; i < sizeof(apps) / sizeof(apps[0]); i++) {
        lv_obj_t *icon = sui_app_icon_create(grid, apps[i].glyph, apps[i].label, apps[i].color);
        lv_obj_add_event_cb(icon, open_app, LV_EVENT_CLICKED, (void *)&apps[i]);
    }
}

const sui_screen_def_t desktop_screen = {
    .name = "desktop",
    .create = create,
};
