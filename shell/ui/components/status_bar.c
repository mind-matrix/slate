#include "components/status_bar.h"

#include <string.h>
#include <time.h>
#include "components/icon.h"
#include "theme/theme.h"

#define CLOCK_PERIOD_MS 1000
#define ALARM_GLYPH 20
#define DOT_SIZE 8
#define HANDLE_W 48
#define HANDLE_H 5
#define HANDLE_BOTTOM 2

typedef struct {
    void (*on_pull)(void);
    lv_obj_t *wifi;
    lv_obj_t *bluetooth;
    lv_obj_t *bell;
    lv_obj_t *dot;
    lv_obj_t *alarm;
} status_bar_t;

static status_bar_t status;

static void update_clock(lv_timer_t *timer)
{
    lv_obj_t *label = lv_timer_get_user_data(timer);
    time_t now = time(NULL);
    struct tm local;
    char text[8];

    localtime_r(&now, &local);
    strftime(text, sizeof(text), "%H:%M", &local);
    if (strcmp(text, lv_label_get_text(label)) != 0)
        lv_label_set_text(label, text);
}

static lv_obj_t *add_icon(lv_obj_t *parent, const char *symbol)
{
    lv_obj_t *icon = lv_label_create(parent);
    lv_label_set_text(icon, symbol);
    lv_obj_set_style_text_color(icon, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    return icon;
}

static void pull(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_active()) != LV_DIR_BOTTOM)
        return;
    if (status.on_pull)
        status.on_pull();
}

lv_obj_t *sui_status_bar_create(lv_obj_t *parent, void (*on_pull)(void))
{
    status.on_pull = on_pull;

    lv_obj_t *bar = sui_box_create(parent);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_size(bar, LV_PCT(100), SUI_STATUS_BAR_H);
    lv_obj_set_style_pad_hor(bar, SUI_SPACE_XL, 0);
    lv_obj_set_style_text_font(bar, sui_font(SUI_TEXT_BODY), 0);
    lv_obj_set_style_text_color(bar, lv_color_hex(SUI_COLOR_TEXT), 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(bar, pull, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(bar, pull, LV_EVENT_GESTURE, NULL);

    lv_obj_t *clock = lv_label_create(bar);
    lv_label_set_text(clock, "");
    lv_timer_t *timer = lv_timer_create(update_clock, CLOCK_PERIOD_MS, clock);
    update_clock(timer);

    lv_obj_t *handle = sui_box_create(bar);
    lv_obj_add_flag(handle, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(handle, HANDLE_W, HANDLE_H);
    lv_obj_set_style_radius(handle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(handle, lv_color_hex(SUI_COLOR_TRACK), 0);
    lv_obj_set_style_bg_opa(handle, LV_OPA_COVER, 0);
    lv_obj_align(handle, LV_ALIGN_BOTTOM_MID, 0, -HANDLE_BOTTOM);

    lv_obj_t *icons = sui_box_create(bar);
    lv_obj_set_flex_flow(icons, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(icons, SUI_SPACE_L, 0);
    lv_obj_set_flex_align(icons, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    status.bell = add_icon(icons, LV_SYMBOL_BELL);
    status.dot = sui_box_create(status.bell);
    lv_obj_add_flag(status.dot, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(status.dot, DOT_SIZE, DOT_SIZE);
    lv_obj_set_style_radius(status.dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(status.dot, lv_color_hex(SUI_COLOR_CAT_SOUND), 0);
    lv_obj_set_style_bg_opa(status.dot, LV_OPA_COVER, 0);
    lv_obj_align(status.dot, LV_ALIGN_TOP_RIGHT, 2, -1);
    status.alarm = sui_glyph_create(icons, SUI_VECTOR(SUI_ICON_ALARM), ALARM_GLYPH, SUI_COLOR_TEXT_MUTED);
    lv_obj_add_flag(status.bell, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(status.alarm, LV_OBJ_FLAG_HIDDEN);
    status.wifi = add_icon(icons, LV_SYMBOL_WIFI);
    status.bluetooth = add_icon(icons, LV_SYMBOL_BLUETOOTH);
    add_icon(icons, LV_SYMBOL_VOLUME_MAX);
    return bar;
}

static void show(lv_obj_t *icon, bool visible)
{
    if (visible)
        lv_obj_remove_flag(icon, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(icon, LV_OBJ_FLAG_HIDDEN);
}

void sui_status_bar_set_radios(bool wifi, bool bluetooth)
{
    if (!status.wifi)
        return;
    show(status.wifi, wifi);
    show(status.bluetooth, bluetooth);
}

void sui_status_bar_set_alerts(bool notifications, bool unseen, bool alarm)
{
    if (!status.bell)
        return;
    show(status.bell, notifications);
    show(status.dot, unseen);
    show(status.alarm, alarm);
}
