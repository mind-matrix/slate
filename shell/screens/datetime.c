#include "datetime.h"

#include <stdio.h>
#include <time.h>
#include "components/header.h"
#include "components/icon.h"
#include "components/list.h"
#include "theme/theme.h"
#include "timezone.h"
#include "timezone_picker.h"

#define CLOCK_CARD_H 136
#define CLOCK_PAD_HOR 32
#define CLOCK_ICON_TILE 64
#define CLOCK_ICON 34
#define CLOCK_ICON_RADIUS 18
#define TICK_MS 1000
#define TEXT 96

typedef struct {
    lv_obj_t *time;
    lv_obj_t *date;
    lv_obj_t *zone_row;
    lv_obj_t *sync_row;
    lv_timer_t *timer;
} datetime_view_t;

static datetime_view_t view;

void datetime_zone_summary(char *buf, size_t len)
{
    char id[TIMEZONE_ID_MAX], city[TIMEZONE_ID_MAX], offset[24];
    timezone_current(id, sizeof(id));
    timezone_city(id, city, sizeof(city));
    timezone_format_offset(timezone_offset(id), offset, sizeof(offset));
    snprintf(buf, len, "%s · %s", city, offset);
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    time_t now = time(NULL);
    struct tm local;
    char text[TEXT];

    localtime_r(&now, &local);
    strftime(text, sizeof(text), "%H:%M", &local);
    lv_label_set_text(view.time, text);
    strftime(text, sizeof(text), "%A, %-d %B %Y", &local);
    lv_label_set_text(view.date, text);
}

static void refresh(void)
{
    char summary[TEXT];
    datetime_zone_summary(summary, sizeof(summary));
    sui_list_row_set_detail(view.zone_row, summary);
    sui_list_row_set_detail(view.sync_row, timezone_ntp_synced() ? "Synced" : "Not synced");
    tick(NULL);
}

static void open_picker(lv_event_t *e)
{
    (void)e;
    sui_nav_push(&timezone_picker_screen, NULL);
}

static lv_obj_t *create_clock_card(lv_obj_t *parent)
{
    lv_obj_t *card = sui_box_create(parent);
    lv_obj_set_size(card, SUI_CONTENT_W, CLOCK_CARD_H);
    lv_obj_set_style_pad_hor(card, CLOCK_PAD_HOR, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(SUI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(SUI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, SUI_RADIUS_ROW, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *text = sui_box_create(card);
    lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(text, SUI_SPACE_XS, 0);
    view.time = lv_label_create(text);
    lv_obj_set_style_text_font(view.time, sui_font(SUI_TEXT_DISPLAY), 0);
    view.date = lv_label_create(text);
    lv_obj_set_style_text_color(view.date, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);

    lv_obj_t *tile = sui_box_create(card);
    lv_obj_set_size(tile, CLOCK_ICON_TILE, CLOCK_ICON_TILE);
    lv_obj_set_style_radius(tile, CLOCK_ICON_RADIUS, 0);
    lv_obj_set_style_bg_color(tile, lv_color_hex(SUI_COLOR_CAT_SYSTEM), 0);
    lv_obj_set_style_bg_opa(tile, SUI_TINT_OPA, 0);
    lv_obj_center(sui_glyph_create(tile, SUI_VECTOR(SUI_ICON_CLOCK), CLOCK_ICON, SUI_COLOR_CAT_SYSTEM));
    return card;
}

static void create(lv_obj_t *screen, void *args)
{
    (void)args;
    lv_obj_t *page = sui_page_create(screen, "Date & time");
    create_clock_card(page);

    lv_obj_t *list = sui_list_create(page, SUI_CONTENT_W);
    sui_row_t zone = { .title = "Time zone", .detail = "", .trailing = SUI_TRAILING_CHEVRON };
    view.zone_row = sui_list_row_create(list, &zone);
    lv_obj_add_event_cb(view.zone_row, open_picker, LV_EVENT_CLICKED, NULL);
    sui_row_t sync = { .title = "Time sync", .detail = "", .trailing = SUI_TRAILING_VALUE };
    view.sync_row = sui_list_row_create(list, &sync);
    refresh();
}

static void on_show(lv_obj_t *screen)
{
    (void)screen;
    refresh();
    view.timer = lv_timer_create(tick, TICK_MS, NULL);
}

static void on_hide(lv_obj_t *screen)
{
    (void)screen;
    lv_timer_delete(view.timer);
    view.timer = NULL;
}

const sui_screen_def_t datetime_screen = {
    .name = "datetime",
    .create = create,
    .on_show = on_show,
    .on_hide = on_hide,
};
