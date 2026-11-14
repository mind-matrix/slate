#include "open_apps.h"

#include <stdio.h>
#include <string.h>

#include "apps.h"
#include "apps_screen.h"
#include "components/app_logo.h"
#include "platform/windows.h"
#include "theme/theme.h"

#define CHIP_H 72
#define CHIP_RADIUS 18
#define CHIP_GAP 12
#define LOGO 48
#define CLOSE_SIZE 48
#define HEADER_H 40

typedef struct {
    lv_obj_t *section;
    lv_obj_t *row;
    lv_obj_t *close_all;
    void (*on_open)(void);
    app_t apps[APPS_MAX];
    size_t count;
    /* Apps just closed: their windows may still be mapped for a moment. */
    char closed[APPS_MAX][APPS_ID_MAX];
    char signature[APPS_MAX * (APPS_ID_MAX + 2) + 8];
    bool row_built;
} open_apps_t;

static open_apps_t view;

static bool just_closed(const char *id)
{
    for (size_t i = 0; i < APPS_MAX; i++)
        if (view.closed[i][0] && strcmp(view.closed[i], id) == 0)
            return true;
    return false;
}

static void remember_closed(const char *id)
{
    for (size_t i = 0; i < APPS_MAX; i++)
        if (!view.closed[i][0]) {
            snprintf(view.closed[i], sizeof(view.closed[i]), "%s", id);
            return;
        }
}

static void on_tap(lv_event_t *e)
{
    const app_t *app = lv_event_get_user_data(e);
    apps_open(app->id);
    if (view.on_open)
        view.on_open();
}

static void on_close(lv_event_t *e)
{
    const app_t *app = lv_event_get_user_data(e);
    apps_stop(app->id);
    remember_closed(app->id);
    open_apps_refresh(view.section);
}

static void on_close_all(lv_event_t *e)
{
    (void)e;
    for (size_t i = 0; i < view.count; i++)
        if (!apps_always_on(&view.apps[i])) {
            apps_stop(view.apps[i].id);
            remember_closed(view.apps[i].id);
        }
    open_apps_refresh(view.section);
}

static lv_obj_t *add_label(lv_obj_t *parent, const char *text, sui_text_t font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, sui_font(font), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

static void add_chip(const app_t *app, bool front)
{
    lv_obj_t *chip = sui_box_create(view.row);
    lv_obj_set_height(chip, CHIP_H);
    lv_obj_set_style_radius(chip, CHIP_RADIUS, 0);
    lv_obj_set_style_bg_color(chip, lv_color_hex(SUI_COLOR_RAISED), 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(chip, SUI_SPACE_S + 4, 0);
    lv_obj_set_style_pad_right(chip, SUI_SPACE_S, 0);
    lv_obj_set_style_pad_column(chip, 14, 0);
    lv_obj_set_flex_flow(chip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chip, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(chip, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SNAPPABLE);
    lv_obj_add_style(chip, sui_style_pressed(), LV_STATE_PRESSED);
    lv_obj_add_event_cb(chip, on_tap, LV_EVENT_CLICKED, (void *)app);

    sui_app_logo_create(chip, app->logo, app->name, LOGO);
    lv_obj_t *texts = sui_box_create(chip);
    lv_obj_set_flex_flow(texts, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(texts, 2, 0);
    lv_obj_set_style_pad_right(texts, SUI_SPACE_S, 0);
    add_label(texts, app->name, SUI_TEXT_ROW, SUI_COLOR_TEXT);
    bool always_on = apps_always_on(app);
    add_label(texts, always_on ? "Always on · stop it in app details" : front ? "Open" : "In the background",
              SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);

    if (always_on)
        return;
    lv_obj_t *close = sui_box_create(chip);
    lv_obj_set_size(close, CLOSE_SIZE, CLOSE_SIZE);
    lv_obj_add_flag(close, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(close, 14, 0);
    lv_obj_add_style(close, sui_style_pressed(), LV_STATE_PRESSED);
    lv_obj_add_event_cb(close, on_close, LV_EVENT_CLICKED, (void *)app);
    lv_obj_center(add_label(close, LV_SYMBOL_CLOSE, SUI_TEXT_ROW, SUI_COLOR_TEXT_MUTED));
}

void open_apps_refresh(lv_obj_t *section)
{
    if (!section || section != view.section)
        return;
    char ids[APPS_MAX][APPS_ID_MAX];
    size_t open = sui_windows_open(&ids[0][0], APPS_ID_MAX, APPS_MAX);
    app_t installed[APPS_MAX];
    size_t installed_count = apps_list(installed, APPS_MAX);
    const char *front = sui_windows_front();

    /* Rebuild only when the open apps change, so a periodic refresh keeps the row's scroll position. */
    char signature[sizeof(view.signature)];
    size_t used = (size_t)snprintf(signature, sizeof(signature), "%s|", front ? front : "");
    for (size_t i = 0; i < open && used < sizeof(signature); i++)
        if (!just_closed(ids[i]))
            used += (size_t)snprintf(signature + used, sizeof(signature) - used, "%s ", ids[i]);
    if (view.row_built && strcmp(signature, view.signature) == 0)
        return;
    snprintf(view.signature, sizeof(view.signature), "%s", signature);
    view.row_built = true;

    lv_obj_clean(view.row);
    view.count = 0;
    bool closable = false;
    for (size_t i = 0; i < open; i++) {
        if (just_closed(ids[i]))
            continue;
        for (size_t j = 0; j < installed_count; j++)
            if (strcmp(installed[j].id, ids[i]) == 0) {
                view.apps[view.count] = installed[j];
                closable |= !apps_always_on(&installed[j]);
                add_chip(&view.apps[view.count], front && strcmp(front, ids[i]) == 0);
                view.count++;
                break;
            }
    }
    /* Forget closed apps once their windows are gone, so a reopened app shows again. */
    for (size_t i = 0; i < APPS_MAX; i++) {
        bool still_open = false;
        for (size_t j = 0; j < open && view.closed[i][0]; j++)
            still_open |= strcmp(view.closed[i], ids[j]) == 0;
        if (!still_open)
            view.closed[i][0] = '\0';
    }
    if (!view.count) {
        lv_obj_t *empty = add_label(view.row, "No apps are open.", SUI_TEXT_BODY, SUI_COLOR_TEXT_MUTED);
        lv_obj_set_height(empty, CHIP_H);
        lv_obj_set_style_pad_top(empty, (CHIP_H - lv_font_get_line_height(sui_font(SUI_TEXT_BODY))) / 2, 0);
        lv_obj_set_style_pad_left(empty, SUI_SPACE_S, 0);
    }
    if (closable)
        lv_obj_remove_flag(view.close_all, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(view.close_all, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *open_apps_create(lv_obj_t *parent, void (*on_open)(void))
{
    memset(&view, 0, sizeof(view));
    view.on_open = on_open;
    lv_obj_t *section = view.section = sui_box_create(parent);
    lv_obj_set_width(section, LV_PCT(100));
    lv_obj_set_flex_flow(section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(section, SUI_SPACE_S, 0);

    lv_obj_t *header = sui_box_create(section);
    lv_obj_set_size(header, LV_PCT(100), HEADER_H);
    lv_obj_set_style_pad_left(header, SUI_SPACE_S, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *title = add_label(header, "OPEN APPS", SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);
    lv_obj_set_style_text_letter_space(title, 1, 0);
    view.close_all = sui_box_create(header);
    lv_obj_set_height(view.close_all, HEADER_H);
    lv_obj_set_style_pad_hor(view.close_all, SUI_SPACE_M, 0);
    lv_obj_add_flag(view.close_all, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_style(view.close_all, sui_style_pressed(), LV_STATE_PRESSED);
    lv_obj_add_event_cb(view.close_all, on_close_all, LV_EVENT_CLICKED, NULL);
    lv_obj_center(add_label(view.close_all, "Close all", SUI_TEXT_BODY, SUI_COLOR_ACCENT));

    /* One row that scrolls sideways, without a scrollbar; a chip cut off at the edge hints at more. */
    lv_obj_t *frame = sui_box_create(section);
    lv_obj_set_size(frame, LV_PCT(100), CHIP_H);
    view.row = sui_box_create(frame);
    lv_obj_set_size(view.row, LV_PCT(100), CHIP_H);
    lv_obj_set_flex_flow(view.row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(view.row, CHIP_GAP, 0);
    lv_obj_add_flag(view.row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(view.row, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(view.row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_x(view.row, LV_SCROLL_SNAP_START);
    lv_obj_remove_flag(view.row, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);

    open_apps_refresh(section);
    return section;
}
