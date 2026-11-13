#include "apps_screen.h"

#include <stdio.h>
#include <string.h>
#include "apps.h"
#include "components/app_logo.h"
#include "components/header.h"
#include "theme/theme.h"

#define ROW_H 96
#define LOGO 56
#define BADGE_H 30
#define TEXT 160

static struct {
    lv_obj_t *list;
    app_t apps[APPS_MAX];
    size_t count;
} view;

static app_t selected;

static lv_obj_t *add_label(lv_obj_t *parent, const char *text, sui_text_t font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, sui_font(font), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

lv_obj_t *apps_badge_create(lv_obj_t *parent, bool dev)
{
    lv_obj_t *badge = sui_box_create(parent);
    lv_obj_set_height(badge, BADGE_H);
    lv_obj_set_style_pad_hor(badge, 12, 0);
    lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(dev ? SUI_COLOR_WARNING_FILL : SUI_COLOR_RAISED), 0);
    lv_obj_set_style_bg_opa(badge, dev ? SUI_TINT_OPA : LV_OPA_COVER, 0);
    lv_obj_t *label = add_label(badge, dev ? "Dev build" : "Built-in", SUI_TEXT_CAPTION,
                                dev ? SUI_COLOR_WARNING_FILL : SUI_COLOR_TEXT_MUTED);
    lv_obj_center(label);
    return badge;
}

static void open_app(lv_event_t *e)
{
    selected = *(const app_t *)lv_event_get_user_data(e);
    sui_nav_push(&app_detail_screen, &selected);
}

static void show(void)
{
    lv_obj_clean(view.list);
    view.count = apps_list(view.apps, APPS_MAX);
    for (size_t i = 0; i < view.count; i++) {
        const app_t *app = &view.apps[i];
        lv_obj_t *row = sui_box_create(view.list);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(row, LV_PCT(100), ROW_H);
        lv_obj_set_style_pad_hor(row, SUI_SPACE_L, 0);
        lv_obj_set_style_pad_column(row, 20, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(SUI_COLOR_RAISED), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
        if (i > 0) {
            lv_obj_set_style_border_side(row, LV_BORDER_SIDE_TOP, 0);
            lv_obj_set_style_border_width(row, 1, 0);
            lv_obj_set_style_border_color(row, lv_color_hex(SUI_COLOR_BORDER), 0);
        }
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_event_cb(row, open_app, LV_EVENT_CLICKED, (void *)app);

        sui_app_logo_create(row, app->logo, app->name, LOGO);
        lv_obj_t *texts = sui_box_create(row);
        lv_obj_set_flex_grow(texts, 1);
        lv_obj_set_flex_flow(texts, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(texts, 3, 0);
        add_label(texts, app->name, SUI_TEXT_ROW, SUI_COLOR_TEXT);
        char detail[TEXT];
        size_t permissions = apps_count(app->granted);
        snprintf(detail, sizeof(detail), "Version %s · %zu permission%s", app->version[0] ? app->version : "unknown",
                 permissions, permissions == 1 ? "" : "s");
        add_label(texts, detail, SUI_TEXT_BODY, SUI_COLOR_TEXT_MUTED);
        apps_badge_create(row, app->dev);
        add_label(row, LV_SYMBOL_RIGHT, SUI_TEXT_ROW, SUI_COLOR_TEXT_MUTED);
    }
    if (!view.count) {
        lv_obj_t *empty = add_label(view.list, "No apps are installed.", SUI_TEXT_BODY, SUI_COLOR_TEXT_MUTED);
        lv_obj_set_style_pad_all(empty, SUI_SPACE_L, 0);
    }
}

void apps_summary(char *out, size_t len)
{
    app_t apps[APPS_MAX];
    size_t count = apps_list(apps, APPS_MAX), used = 0;
    out[0] = '\0';
    for (size_t i = 0; i < count && used < len; i++)
        used += (size_t)snprintf(out + used, len - used, "%s%s", i ? ", " : "", apps[i].name);
    if (!count)
        snprintf(out, len, "No apps installed");
}

static void create(lv_obj_t *screen, void *args)
{
    (void)args;
    lv_obj_t *page = sui_page_create(screen, "Apps");
    lv_obj_add_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(page, LV_DIR_VER);
    view.list = sui_box_create(page);
    lv_obj_set_size(view.list, SUI_CONTENT_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(view.list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(view.list, lv_color_hex(SUI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(view.list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(view.list, lv_color_hex(SUI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(view.list, 1, 0);
    lv_obj_set_style_radius(view.list, SUI_RADIUS_ROW, 0);
    lv_obj_set_style_clip_corner(view.list, true, 0);
    lv_obj_t *note = add_label(page, "Apps update separately from Slate. Updates are listed under About › Software update.",
                               SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);
    lv_obj_set_width(note, SUI_CONTENT_W);
    lv_obj_set_style_pad_left(note, SUI_SPACE_M, 0);
}

static void on_show(lv_obj_t *screen)
{
    (void)screen;
    show();
}

const sui_screen_def_t apps_screen = {
    .name = "apps",
    .create = create,
    .on_show = on_show,
};
