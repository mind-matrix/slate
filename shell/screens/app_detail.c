#include <stdio.h>
#include <string.h>

#include "apps.h"
#include "apps_screen.h"
#include "components/app_logo.h"
#include "components/button.h"
#include "components/header.h"
#include "platform/platform.h"
#include "theme/theme.h"

#define CONTENT_Y (SUI_STATUS_BAR_H + SUI_HEADER_H)
#define CARD_W 440
#define LOGO 96
#define PERMISSION_H 80
#define TEXT 96

static struct {
    app_t app;
    lv_obj_t *status;
} view;

static lv_obj_t *add_label(lv_obj_t *parent, const char *text, sui_text_t font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, sui_font(font), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

static lv_obj_t *add_card(lv_obj_t *parent)
{
    lv_obj_t *card = sui_box_create(parent);
    lv_obj_set_style_radius(card, SUI_RADIUS_ROW, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(SUI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(SUI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    return card;
}

static lv_obj_t *add_fact(lv_obj_t *list, const char *key, const char *value)
{
    lv_obj_t *row = sui_box_create(list);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    add_label(row, key, SUI_TEXT_BODY, SUI_COLOR_TEXT_MUTED);
    return add_label(row, value, SUI_TEXT_BODY, SUI_COLOR_TEXT);
}

static void show_status(void)
{
    bool running = apps_running(view.app.id);
    lv_label_set_text(view.status, running ? "Running" : "Stopped");
    lv_obj_set_style_text_color(view.status, lv_color_hex(running ? SUI_COLOR_SUCCESS : SUI_COLOR_TEXT_MUTED), 0);
}

static void on_open(lv_event_t *e)
{
    (void)e;
    apps_open(view.app.id);
    show_status();
}

static void on_restart(lv_event_t *e)
{
    (void)e;
    apps_restart(view.app.id);
    show_status();
}

static void create_card(lv_obj_t *parent)
{
    const app_t *app = &view.app;
    lv_obj_t *card = add_card(parent);
    lv_obj_set_size(card, CARD_W, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(card, 28, 0);
    lv_obj_set_style_pad_row(card, 22, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *top = sui_box_create(card);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(top, 20, 0);
    sui_app_logo_create(top, app->logo, app->name, LOGO);
    lv_obj_t *titles = sui_box_create(top);
    lv_obj_set_flex_flow(titles, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(titles, 6, 0);
    add_label(titles, app->name, SUI_TEXT_HEADING, SUI_COLOR_TEXT);
    apps_badge_create(titles, app->dev);

    lv_obj_t *facts = sui_box_create(card);
    lv_obj_set_width(facts, LV_PCT(100));
    lv_obj_set_flex_flow(facts, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(facts, 12, 0);
    add_fact(facts, "Version", app->version[0] ? app->version : "Unknown");
    add_fact(facts, "Source", app->dev ? "Local checkout" : "GitHub releases");
    add_fact(facts, "Updates", app->dev ? "Off for dev builds" : "With Software update");
    view.status = add_fact(facts, "Status", "");
    show_status();

    lv_obj_t *buttons = sui_box_create(card);
    lv_obj_set_flex_flow(buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(buttons, 12, 0);
    if (apps_has(app->granted, "gui")) {
        lv_obj_t *open = sui_button_create(buttons, "Open", SUI_COLOR_ACCENT, SUI_COLOR_ON_ACCENT);
        lv_obj_add_event_cb(open, on_open, LV_EVENT_CLICKED, NULL);
    }
    lv_obj_t *restart = sui_button_secondary_create(buttons, "Restart app");
    lv_obj_add_event_cb(restart, on_restart, LV_EVENT_CLICKED, NULL);
}

static void create_permissions(lv_obj_t *parent)
{
    lv_obj_t *section = sui_box_create(parent);
    lv_obj_set_height(section, LV_PCT(100));
    lv_obj_set_flex_grow(section, 1);
    lv_obj_set_style_pad_row(section, 10, 0);
    lv_obj_set_flex_flow(section, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *title = add_label(section, "PERMISSIONS GRANTED BY SLATE", SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);
    lv_obj_set_style_text_letter_space(title, 1, 0);
    lv_obj_set_style_pad_left(title, SUI_SPACE_S, 0);

    lv_obj_t *grid = add_card(section);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_flex_grow(grid, 1);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_clip_corner(grid, true, 0);
    lv_obj_add_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(grid, LV_DIR_VER);

    char list[APPS_PERMISSIONS_MAX];
    snprintf(list, sizeof(list), "%s", view.app.granted);
    size_t index = 0;
    for (char *save = NULL, *word = strtok_r(list, " \n", &save); word; word = strtok_r(NULL, " \n", &save)) {
        char name[TEXT], detail[TEXT];
        if (!apps_permission_text(word, name, sizeof(name), detail, sizeof(detail)))
            continue;
        lv_obj_t *cell = sui_box_create(grid);
        lv_obj_set_size(cell, LV_PCT(50), PERMISSION_H);
        lv_obj_set_style_pad_hor(cell, 20, 0);
        lv_obj_set_style_pad_row(cell, 3, 0);
        lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_border_color(cell, lv_color_hex(SUI_COLOR_BORDER), 0);
        lv_obj_set_style_border_width(cell, 1, 0);
        lv_obj_set_style_border_side(cell, (index > 1 ? LV_BORDER_SIDE_TOP : 0) | (index % 2 ? LV_BORDER_SIDE_LEFT : 0),
                                     0);
        add_label(cell, name, SUI_TEXT_ROW, SUI_COLOR_TEXT);
        add_label(cell, detail, SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);
        index++;
    }
    if (!index)
        lv_obj_set_style_pad_all(add_label(grid, "No special permissions.", SUI_TEXT_BODY, SUI_COLOR_TEXT_MUTED),
                                 SUI_SPACE_L, 0);
}

static void create(lv_obj_t *screen, void *args)
{
    view.app = *(const app_t *)args;
    sui_header_create(screen, view.app.name);
    lv_obj_t *content = sui_box_create(screen);
    lv_obj_set_size(content, LV_PCT(100), SUI_SCREEN_H - CONTENT_Y);
    lv_obj_set_pos(content, 0, CONTENT_Y);
    lv_obj_set_style_pad_top(content, SUI_SPACE_S, 0);
    lv_obj_set_style_pad_hor(content, SUI_SPACE_XL, 0);
    lv_obj_set_style_pad_bottom(content, SUI_SPACE_L, 0);
    lv_obj_set_style_pad_column(content, SUI_SPACE_L, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    create_card(content);
    create_permissions(content);
}

const sui_screen_def_t app_detail_screen = {
    .name = "app_detail",
    .create = create,
};
