#include "clock_app.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "audio.h"
#include "clock.h"
#include "clock_alerts.h"
#include "clock_sheets.h"
#include "components/button.h"
#include "components/header.h"
#include "components/icon.h"
#include "components/roller.h"
#include "components/segmented.h"
#include "components/switch.h"
#include "platform/platform.h"
#include "theme/theme.h"

#define CONTENT_Y (SUI_STATUS_BAR_H + SUI_HEADER_H)
#define CONTENT_H (SUI_SCREEN_H - CONTENT_Y)
#define TABS_W 640
#define ADD_W 196
#define CARD_H 120
#define CARD_GAP 16
#define CARD_W ((SUI_SCREEN_W - 2 * SUI_SPACE_XL - CARD_GAP) / 2)
#define SETUP_W 520
#define ROLLER_W 128
#define PRESET_COUNT 5
#define BIG_BUTTON_H 64
#define ROUND 56
#define LAPS_W 420
#define LAP_ROW_H 56
#define CHECK_TOUCH 72
#define CHECK_SIZE 30
#define TICK_FAST_MS 100
#define TICK_SLOW_MS 1000
#define TEXT 192
#define DAY_SECONDS 86400

enum { TAB_ALARMS, TAB_TIMERS, TAB_STOPWATCH, TAB_REMINDERS, TAB_COUNT };

typedef struct {
    uint32_t id;
    lv_obj_t *time;
    lv_obj_t *sub;
    lv_obj_t *bar;
} timer_row_t;

typedef struct {
    lv_obj_t *tabs;
    lv_obj_t *add;
    lv_obj_t *panes[TAB_COUNT];
    lv_obj_t *next;
    lv_obj_t *output;
    lv_obj_t *alarms;
    lv_obj_t *rollers[3];
    lv_obj_t *start;
    lv_obj_t *timers_title;
    lv_obj_t *timers;
    lv_obj_t *timers_empty;
    timer_row_t rows[CLOCK_MAX_TIMERS];
    size_t row_count;
    lv_obj_t *sw_main;
    lv_obj_t *sw_frac;
    lv_obj_t *sw_primary;
    lv_obj_t *sw_secondary;
    lv_obj_t *laps_title;
    lv_obj_t *laps;
    lv_obj_t *laps_empty;
    size_t laps_shown;
    lv_obj_t *reminders;
    lv_timer_t *timer;
    bool visible;
} clock_view_t;

static clock_view_t view;
static size_t current_tab;

static const char *const tab_labels[TAB_COUNT] = { "Alarms", "Timers", "Stopwatch", "Reminders" };
static const int presets[PRESET_COUNT] = { 1, 3, 5, 10, 15 };

static void refresh_all(void);

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

static lv_obj_t *add_section_title(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = add_label(parent, text, SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);
    lv_obj_set_style_text_letter_space(label, 1, 0);
    lv_obj_set_style_pad_left(label, SUI_SPACE_S, 0);
    return label;
}

static lv_obj_t *add_round(lv_obj_t *parent, const char *symbol, uint32_t fill, uint32_t ink, lv_event_cb_t cb,
                           void *user)
{
    lv_obj_t *button = sui_box_create(parent);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(button, ROUND, ROUND);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(fill), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_add_style(button, sui_style_pressed(), LV_STATE_PRESSED);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user);
    lv_obj_t *glyph = sui_glyph_create(button, SUI_SYMBOL(symbol), 22, ink);
    lv_obj_center(glyph);
    return button;
}

static lv_obj_t *add_pill(lv_obj_t *parent, const char *text, uint32_t fill, uint32_t ink, lv_event_cb_t cb)
{
    lv_obj_t *button = sui_button_create(parent, text, fill, ink);
    lv_obj_set_size(button, 200, BIG_BUTTON_H);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_text_font(button, sui_font(SUI_TEXT_ROW), 0);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);
    return button;
}

static void set_pill(lv_obj_t *button, const char *text, uint32_t fill, uint32_t ink)
{
    sui_button_set_text(button, text);
    lv_obj_set_style_bg_color(button, lv_color_hex(fill), 0);
    lv_obj_set_style_text_color(button, lv_color_hex(ink), 0);
}

static void set_visible(lv_obj_t *obj, bool visible)
{
    if (visible)
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *add_pane(lv_obj_t *screen, lv_flex_flow_t flow)
{
    lv_obj_t *pane = sui_box_create(screen);
    lv_obj_set_size(pane, LV_PCT(100), CONTENT_H);
    lv_obj_set_pos(pane, 0, CONTENT_Y);
    lv_obj_set_style_pad_top(pane, SUI_SPACE_S, 0);
    lv_obj_set_style_pad_hor(pane, SUI_SPACE_XL, 0);
    lv_obj_set_style_pad_bottom(pane, SUI_SPACE_L, 0);
    lv_obj_set_style_pad_row(pane, SUI_SPACE_M, 0);
    lv_obj_set_style_pad_column(pane, SUI_SPACE_L, 0);
    lv_obj_set_flex_flow(pane, flow);
    return pane;
}

static void format_clock_time(int64_t when, char *out, size_t len)
{
    time_t t = (time_t)when;
    struct tm local;
    localtime_r(&t, &local);
    strftime(out, len, "%H:%M", &local);
}

static long local_day(int64_t when)
{
    time_t t = (time_t)when;
    struct tm local;
    localtime_r(&t, &local);
    return (long)local.tm_year * 400 + local.tm_yday;
}

static void format_day(int64_t when, int64_t now, char *out, size_t len)
{
    long day = local_day(when), today = local_day(now);
    if (day == today)
        snprintf(out, len, "Today");
    else if (day == local_day(now + DAY_SECONDS))
        snprintf(out, len, "Tomorrow");
    else if (when < now) {
        snprintf(out, len, "Earlier");
    } else {
        time_t t = (time_t)when;
        struct tm local;
        localtime_r(&t, &local);
        strftime(out, len, "%a %-d %b", &local);
    }
}

static void next_alarm_text(char *out, size_t len)
{
    if (!clock_time_synced()) {
        snprintf(out, len, "Time not set yet. Alarms wait until it is.");
        return;
    }
    const clock_state_t *s = clock_state();
    int64_t now = (int64_t)time(NULL), best = 0;
    for (size_t i = 0; i < s->alarm_count; i++) {
        if (!s->alarms[i].on)
            continue;
        int64_t at = clock_alarm_next(&s->alarms[i], now);
        if (at && (!best || at < best))
            best = at;
    }
    if (!best) {
        snprintf(out, len, "No alarms on");
        return;
    }
    char day[32], at[8], until[CLOCK_FORMAT_MAX];
    format_day(best, now, day, sizeof(day));
    if (strcmp(day, "Today") == 0 || strcmp(day, "Tomorrow") == 0)
        day[0] = (char)(day[0] - 'A' + 'a');
    else {
        time_t t = (time_t)best;
        struct tm local;
        localtime_r(&t, &local);
        strftime(day, sizeof(day), "%A", &local);
    }
    format_clock_time(best, at, sizeof(at));
    clock_format_until(best - now, until, sizeof(until));
    snprintf(out, len, "Next alarm %s at %s · in %s", day, at, until);
}

static void output_text(char *out, size_t len)
{
    audio_device_t devices[AUDIO_MAX_DEVICES];
    size_t count = audio_list(AUDIO_OUTPUT, devices, AUDIO_MAX_DEVICES);
    for (size_t i = 0; i < count; i++) {
        if (devices[i].is_default) {
            snprintf(out, len, "Rings on %s", devices[i].name);
            return;
        }
    }
    snprintf(out, len, "No speaker connected · alarms light up the screen");
}

static void on_alarm_switch(lv_event_t *e)
{
    clock_alarm_t *alarm = clock_find_alarm((uint32_t)(uintptr_t)lv_event_get_user_data(e));
    if (!alarm)
        return;
    alarm->on = sui_switch_get(lv_event_get_target(e));
    alarm->snoozed_until = 0;
    clock_alerts_handled(CLOCK_KEY_ALARM, alarm->id);
    clock_save();
    refresh_all();
}

static void on_alarm_open(lv_event_t *e)
{
    clock_sheet_alarm_open((uint32_t)(uintptr_t)lv_event_get_user_data(e), clock_app_refresh);
}

static void build_alarms(void)
{
    char text[TEXT];
    next_alarm_text(text, sizeof(text));
    lv_label_set_text(view.next, text);
    lv_obj_clean(view.alarms);

    const clock_state_t *s = clock_state();
    if (!s->alarm_count) {
        add_label(view.alarms, "No alarms yet. Tap Add alarm.", SUI_TEXT_BODY, SUI_COLOR_TEXT_MUTED);
        return;
    }
    for (size_t i = 0; i < s->alarm_count; i++) {
        const clock_alarm_t *a = &s->alarms[i];
        uint32_t ink = a->on ? SUI_COLOR_TEXT : SUI_COLOR_TRACK;
        void *id = (void *)(uintptr_t)a->id;
        lv_obj_t *card = add_card(view.alarms);
        lv_obj_set_size(card, CARD_W, CARD_H);
        lv_obj_set_style_pad_right(card, SUI_SPACE_L, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *body = sui_box_create(card);
        lv_obj_add_flag(body, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_height(body, LV_PCT(100));
        lv_obj_set_flex_grow(body, 1);
        lv_obj_set_style_pad_left(body, 28, 0);
        lv_obj_set_style_pad_row(body, SUI_SPACE_XS, 0);
        lv_obj_set_style_radius(body, SUI_RADIUS_ROW, 0);
        lv_obj_add_style(body, sui_style_pressed(), LV_STATE_PRESSED);
        lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_add_event_cb(body, on_alarm_open, LV_EVENT_CLICKED, id);

        char time_text[8], days[CLOCK_FORMAT_MAX];
        snprintf(time_text, sizeof(time_text), "%02u:%02u", a->hour, a->minute);
        add_label(body, time_text, SUI_TEXT_DISPLAY, ink);
        clock_format_days(a->days, days, sizeof(days));
        snprintf(text, sizeof(text), "%s%s%s", a->label, a->label[0] ? " · " : "", days);
        lv_obj_t *sub = add_label(body, text, SUI_TEXT_BODY, a->on ? SUI_COLOR_TEXT_MUTED : SUI_COLOR_TRACK);
        lv_label_set_long_mode(sub, LV_LABEL_LONG_DOT);
        lv_obj_set_width(sub, LV_PCT(100));

        lv_obj_t *sw = sui_switch_create(card, SUI_COLOR_CAT_CLOCK);
        sui_switch_set(sw, a->on);
        lv_obj_add_event_cb(sw, on_alarm_switch, LV_EVENT_VALUE_CHANGED, id);
    }
}

static void create_alarms(lv_obj_t *screen)
{
    lv_obj_t *pane = view.panes[TAB_ALARMS] = add_pane(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *top = sui_box_create(pane);
    lv_obj_set_size(top, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(top, SUI_SPACE_S, 0);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    view.next = add_label(top, "", SUI_TEXT_ROW, SUI_COLOR_TEXT);
    view.output = add_label(top, "", SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);

    view.alarms = sui_box_create(pane);
    lv_obj_set_width(view.alarms, LV_PCT(100));
    lv_obj_set_flex_grow(view.alarms, 1);
    lv_obj_set_style_pad_row(view.alarms, CARD_GAP, 0);
    lv_obj_set_style_pad_column(view.alarms, CARD_GAP, 0);
    lv_obj_set_flex_flow(view.alarms, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_add_flag(view.alarms, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(view.alarms, LV_DIR_VER);
}

static int64_t setup_ms(void)
{
    int h = sui_roller_get(view.rollers[0]), m = sui_roller_get(view.rollers[1]), s = sui_roller_get(view.rollers[2]);
    return ((int64_t)h * 3600 + m * 60 + s) * 1000;
}

static void show_start(void)
{
    bool ready = setup_ms() > 0;
    lv_obj_set_style_bg_color(view.start, lv_color_hex(ready ? SUI_COLOR_CAT_CLOCK : SUI_COLOR_RAISED), 0);
    lv_obj_set_style_text_color(view.start, lv_color_hex(ready ? SUI_COLOR_ON_ACCENT : SUI_COLOR_TRACK), 0);
}

static void on_roller(lv_event_t *e)
{
    (void)e;
    show_start();
}

static void on_preset(lv_event_t *e)
{
    int minutes = (int)(uintptr_t)lv_event_get_user_data(e);
    sui_roller_set(view.rollers[0], 0);
    sui_roller_set(view.rollers[1], minutes);
    sui_roller_set(view.rollers[2], 0);
    show_start();
}

static void on_start_timer(lv_event_t *e)
{
    (void)e;
    if (clock_add_timer(setup_ms(), "")) {
        clock_save();
        refresh_all();
    }
}

static clock_timer_t *row_timer(lv_event_t *e)
{
    return clock_find_timer((uint32_t)(uintptr_t)lv_event_get_user_data(e));
}

static void on_timer_toggle(lv_event_t *e)
{
    clock_timer_t *t = row_timer(e);
    if (!t)
        return;
    if (t->deadline)
        clock_timer_pause(t, clock_mono_ms());
    else
        clock_timer_resume(t, clock_mono_ms());
    clock_save();
    refresh_all();
}

static void on_timer_add(lv_event_t *e)
{
    clock_timer_t *t = row_timer(e);
    if (!t)
        return;
    clock_timer_add(t, 60000, clock_mono_ms());
    clock_alerts_handled(CLOCK_KEY_TIMER, t->id);
    clock_save();
    refresh_all();
}

static void on_timer_cancel(lv_event_t *e)
{
    clock_timer_t *t = row_timer(e);
    if (!t)
        return;
    clock_alerts_handled(CLOCK_KEY_TIMER, t->id);
    clock_delete_timer(t->id);
    clock_save();
    refresh_all();
}

static void timer_name(const clock_timer_t *t, char *out, size_t len)
{
    char total[CLOCK_FORMAT_MAX];
    clock_format_duration(t->total_ms, total, sizeof(total));
    if (t->label[0])
        snprintf(out, len, "%s · %s", t->label, total);
    else
        snprintf(out, len, "%s timer", total);
}

static void update_timer_rows(void)
{
    int64_t mono = clock_mono_ms();
    for (size_t i = 0; i < view.row_count; i++) {
        timer_row_t *row = &view.rows[i];
        const clock_timer_t *t = clock_find_timer(row->id);
        if (!t)
            continue;
        int64_t left = clock_timer_left(t, mono);
        char text[TEXT], name[CLOCK_LABEL_MAX + CLOCK_FORMAT_MAX + 8];
        clock_format_duration(left, text, sizeof(text));
        lv_label_set_text(row->time, text);
        timer_name(t, name, sizeof(name));
        if (!left)
            snprintf(text, sizeof(text), "%s · Done", name);
        else if (t->deadline) {
            char at[8];
            format_clock_time((int64_t)time(NULL) + (left + 999) / 1000, at, sizeof(at));
            snprintf(text, sizeof(text), "%s · Ends at %s", name, at);
        } else
            snprintf(text, sizeof(text), "%s · Paused", name);
        lv_label_set_text(row->sub, text);
        int32_t pct = t->total_ms ? (int32_t)(100 - left * 100 / t->total_ms) : 100;
        lv_bar_set_value(row->bar, pct, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(row->bar, lv_color_hex(t->deadline ? SUI_COLOR_CAT_CLOCK : SUI_COLOR_TEXT_MUTED),
                                  LV_PART_INDICATOR);
        lv_obj_set_style_text_color(row->time, lv_color_hex(t->deadline || !left ? SUI_COLOR_TEXT : SUI_COLOR_TEXT_MUTED),
                                    0);
    }
}

static void build_timers(void)
{
    const clock_state_t *s = clock_state();
    char title[48];
    snprintf(title, sizeof(title), s->timer_count ? "TIMERS · %zu" : "TIMERS", s->timer_count);
    lv_label_set_text(view.timers_title, title);
    set_visible(view.timers_empty, !s->timer_count);
    set_visible(view.timers, s->timer_count);
    lv_obj_clean(view.timers);
    view.row_count = 0;
    int64_t mono = clock_mono_ms();
    for (size_t i = 0; i < s->timer_count; i++) {
        const clock_timer_t *t = &s->timers[i];
        void *id = (void *)(uintptr_t)t->id;
        timer_row_t *row = &view.rows[view.row_count++];
        row->id = t->id;

        lv_obj_t *card = add_card(view.timers);
        lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(card, 18, 0);
        lv_obj_set_style_pad_left(card, SUI_SPACE_L, 0);
        lv_obj_set_style_pad_row(card, 12, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

        lv_obj_t *top = sui_box_create(card);
        lv_obj_set_size(top, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_column(top, SUI_SPACE_M, 0);
        lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t *texts = sui_box_create(top);
        lv_obj_set_height(texts, LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(texts, 1);
        lv_obj_set_flex_flow(texts, LV_FLEX_FLOW_COLUMN);
        row->time = add_label(texts, "", SUI_TEXT_DISPLAY, SUI_COLOR_TEXT);
        row->sub = add_label(texts, "", SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);

        lv_obj_t *plus = sui_button_secondary_create(top, "+1:00");
        lv_obj_add_event_cb(plus, on_timer_add, LV_EVENT_CLICKED, id);
        bool running = t->deadline != 0;
        bool done = clock_timer_left(t, mono) == 0;
        if (!done)
            add_round(top, running ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY, SUI_COLOR_CAT_CLOCK,
                                    SUI_COLOR_ON_ACCENT, on_timer_toggle, id);
        add_round(top, LV_SYMBOL_CLOSE, SUI_COLOR_RAISED, SUI_COLOR_TEXT, on_timer_cancel, id);

        row->bar = lv_bar_create(card);
        lv_obj_set_size(row->bar, LV_PCT(100), 6);
        lv_bar_set_range(row->bar, 0, 100);
        lv_obj_set_style_bg_color(row->bar, lv_color_hex(SUI_COLOR_TRACK), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row->bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(row->bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row->bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_radius(row->bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    }
    update_timer_rows();
}

static void create_timers(lv_obj_t *screen)
{
    lv_obj_t *pane = view.panes[TAB_TIMERS] = add_pane(screen, LV_FLEX_FLOW_ROW);

    lv_obj_t *setup = add_card(pane);
    lv_obj_set_size(setup, SETUP_W, LV_PCT(100));
    lv_obj_set_style_pad_all(setup, SUI_SPACE_L, 0);
    lv_obj_set_style_pad_row(setup, SUI_SPACE_M, 0);
    lv_obj_set_flex_flow(setup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(setup, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *wheels = sui_box_create(setup);
    lv_obj_set_size(wheels, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(wheels, 1);
    lv_obj_set_style_pad_column(wheels, SUI_SPACE_S, 0);
    lv_obj_set_flex_flow(wheels, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wheels, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    static const char *const names[3] = { "HOURS", "MINUTES", "SECONDS" };
    static const int maxima[3] = { 23, 59, 59 };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *column = sui_box_create(wheels);
        lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(column, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(column, SUI_SPACE_XS, 0);
        view.rollers[i] = sui_roller_create(column, maxima[i], ROLLER_W);
        lv_obj_add_event_cb(view.rollers[i], on_roller, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_t *name = add_label(column, names[i], SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);
        lv_obj_set_style_text_letter_space(name, 1, 0);
    }
    sui_roller_set(view.rollers[1], 10);

    lv_obj_t *chips = sui_box_create(setup);
    lv_obj_set_size(chips, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(chips, SUI_SPACE_S, 0);
    lv_obj_set_flex_flow(chips, LV_FLEX_FLOW_ROW);
    for (int i = 0; i < PRESET_COUNT; i++) {
        char text[16];
        snprintf(text, sizeof(text), "%d min", presets[i]);
        lv_obj_t *chip = sui_button_secondary_create(chips, text);
        lv_obj_set_flex_grow(chip, 1);
        lv_obj_set_style_pad_hor(chip, 0, 0);
        lv_obj_add_event_cb(chip, on_preset, LV_EVENT_CLICKED, (void *)(uintptr_t)presets[i]);
    }
    view.start = sui_button_create(setup, "Start timer", SUI_COLOR_CAT_CLOCK, SUI_COLOR_ON_ACCENT);
    lv_obj_set_size(view.start, LV_PCT(100), BIG_BUTTON_H);
    lv_obj_set_style_radius(view.start, SUI_RADIUS_ROW, 0);
    lv_obj_set_style_text_font(view.start, sui_font(SUI_TEXT_ROW), 0);
    lv_obj_add_event_cb(view.start, on_start_timer, LV_EVENT_CLICKED, NULL);
    show_start();

    lv_obj_t *side = sui_box_create(pane);
    lv_obj_set_height(side, LV_PCT(100));
    lv_obj_set_flex_grow(side, 1);
    lv_obj_set_style_pad_row(side, 10, 0);
    lv_obj_set_flex_flow(side, LV_FLEX_FLOW_COLUMN);
    view.timers_title = add_section_title(side, "TIMERS");
    view.timers_empty = add_card(side);
    lv_obj_set_width(view.timers_empty, LV_PCT(100));
    lv_obj_set_flex_grow(view.timers_empty, 1);
    lv_obj_set_style_bg_opa(view.timers_empty, LV_OPA_TRANSP, 0);
    lv_obj_center(add_label(view.timers_empty, "Set a time and tap Start timer.", SUI_TEXT_BODY,
                            SUI_COLOR_TEXT_MUTED));
    view.timers = sui_box_create(side);
    lv_obj_set_width(view.timers, LV_PCT(100));
    lv_obj_set_flex_grow(view.timers, 1);
    lv_obj_set_style_pad_row(view.timers, 12, 0);
    lv_obj_set_flex_flow(view.timers, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(view.timers, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(view.timers, LV_DIR_VER);
}

static void on_sw_primary(lv_event_t *e)
{
    (void)e;
    clock_stopwatch_toggle(clock_stopwatch(), clock_mono_ms());
    refresh_all();
}

static void on_sw_secondary(lv_event_t *e)
{
    (void)e;
    clock_stopwatch_t *sw = clock_stopwatch();
    if (sw->running)
        clock_stopwatch_lap(sw, clock_mono_ms());
    else
        clock_stopwatch_reset(sw);
    refresh_all();
}

static void build_laps(void)
{
    const clock_stopwatch_t *sw = clock_stopwatch();
    char title[48];
    snprintf(title, sizeof(title), sw->lap_count ? "LAPS · %zu" : "LAPS", sw->lap_count);
    lv_label_set_text(view.laps_title, title);
    set_visible(view.laps_empty, !sw->lap_count);
    set_visible(view.laps, sw->lap_count);
    if (view.laps_shown == sw->lap_count && sw->lap_count)
        return;
    view.laps_shown = sw->lap_count;
    lv_obj_clean(view.laps);

    int64_t best = 0, worst = 0;
    for (size_t i = 0; i < sw->lap_count; i++) {
        int64_t split = sw->laps[i] - (i ? sw->laps[i - 1] : 0);
        if (!i || split < best)
            best = split;
        if (!i || split > worst)
            worst = split;
    }
    for (size_t n = sw->lap_count; n > 0; n--) {
        size_t i = n - 1;
        int64_t split = sw->laps[i] - (i ? sw->laps[i - 1] : 0);
        char main[CLOCK_FORMAT_MAX], frac[8], text[TEXT];
        lv_obj_t *row = sui_box_create(view.laps);
        lv_obj_set_size(row, LV_PCT(100), LAP_ROW_H);
        lv_obj_set_style_pad_hor(row, 20, 0);
        lv_obj_set_style_pad_column(row, SUI_SPACE_M, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(SUI_COLOR_BORDER), 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        snprintf(text, sizeof(text), "Lap %zu", n);
        lv_obj_set_width(add_label(row, text, SUI_TEXT_BODY, SUI_COLOR_TEXT_MUTED), 80);
        uint32_t color = sw->lap_count > 1 && split == best    ? SUI_COLOR_SUCCESS
                         : sw->lap_count > 1 && split == worst ? SUI_COLOR_DANGER
                                                               : SUI_COLOR_TEXT;
        clock_format_stopwatch(split, main, sizeof(main), frac, sizeof(frac));
        snprintf(text, sizeof(text), "%s%s", main, frac);
        lv_obj_set_flex_grow(add_label(row, text, SUI_TEXT_ROW, color), 1);
        clock_format_stopwatch(sw->laps[i], main, sizeof(main), frac, sizeof(frac));
        snprintf(text, sizeof(text), "%s%s", main, frac);
        add_label(row, text, SUI_TEXT_BODY, SUI_COLOR_TEXT_MUTED);
    }
}

static void update_stopwatch(void)
{
    const clock_stopwatch_t *sw = clock_stopwatch();
    int64_t elapsed = clock_stopwatch_elapsed(sw, clock_mono_ms());
    char main[CLOCK_FORMAT_MAX], frac[8];
    clock_format_stopwatch(elapsed, main, sizeof(main), frac, sizeof(frac));
    lv_label_set_text(view.sw_main, main);
    lv_label_set_text(view.sw_frac, frac);
    if (sw->running)
        set_pill(view.sw_primary, "Stop", SUI_COLOR_DANGER_FILL, SUI_COLOR_WHITE);
    else
        set_pill(view.sw_primary, elapsed ? "Resume" : "Start", SUI_COLOR_CAT_CLOCK, SUI_COLOR_ON_ACCENT);
    set_pill(view.sw_secondary, sw->running ? "Lap" : "Reset", SUI_COLOR_RAISED,
             sw->running || elapsed ? SUI_COLOR_TEXT : SUI_COLOR_TRACK);
}

static void create_stopwatch(lv_obj_t *screen)
{
    lv_obj_t *pane = view.panes[TAB_STOPWATCH] = add_pane(screen, LV_FLEX_FLOW_ROW);
    lv_obj_t *card = add_card(pane);
    lv_obj_set_height(card, LV_PCT(100));
    lv_obj_set_flex_grow(card, 1);
    lv_obj_set_style_pad_row(card, 40, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *readout = sui_box_create(card);
    lv_obj_set_flex_flow(readout, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(readout, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    view.sw_main = add_label(readout, "00:00", SUI_TEXT_DISPLAY, SUI_COLOR_TEXT);
    lv_obj_set_style_text_font(view.sw_main, sui_font_digits(), 0);
    view.sw_frac = add_label(readout, ".00", SUI_TEXT_DISPLAY, SUI_COLOR_TEXT_MUTED);
    lv_obj_set_style_pad_bottom(view.sw_frac, 12, 0);

    lv_obj_t *buttons = sui_box_create(card);
    lv_obj_set_flex_flow(buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(buttons, 20, 0);
    view.sw_secondary = add_pill(buttons, "Reset", SUI_COLOR_RAISED, SUI_COLOR_TRACK, on_sw_secondary);
    view.sw_primary = add_pill(buttons, "Start", SUI_COLOR_CAT_CLOCK, SUI_COLOR_ON_ACCENT, on_sw_primary);

    lv_obj_t *side = sui_box_create(pane);
    lv_obj_set_size(side, LAPS_W, LV_PCT(100));
    lv_obj_set_style_pad_row(side, 10, 0);
    lv_obj_set_flex_flow(side, LV_FLEX_FLOW_COLUMN);
    view.laps_title = add_section_title(side, "LAPS");
    view.laps_empty = add_card(side);
    lv_obj_set_width(view.laps_empty, LV_PCT(100));
    lv_obj_set_flex_grow(view.laps_empty, 1);
    lv_obj_set_style_bg_opa(view.laps_empty, LV_OPA_TRANSP, 0);
    lv_obj_t *hint = add_label(view.laps_empty, "Tap Lap while the stopwatch runs to record splits.", SUI_TEXT_BODY,
                               SUI_COLOR_TEXT_MUTED);
    lv_obj_set_width(hint, 300);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(hint);
    view.laps = sui_box_create(side);
    lv_obj_set_width(view.laps, LV_PCT(100));
    lv_obj_set_flex_grow(view.laps, 1);
    lv_obj_set_flex_flow(view.laps, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(view.laps, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(view.laps, LV_DIR_VER);
}

static void on_reminder_check(lv_event_t *e)
{
    clock_reminder_t *r = clock_find_reminder((uint32_t)(uintptr_t)lv_event_get_user_data(e));
    if (!r)
        return;
    r->done = !r->done;
    if (r->done)
        clock_alerts_handled(CLOCK_KEY_REMINDER, r->id);
    clock_save();
    refresh_all();
}

static void on_reminder_open(lv_event_t *e)
{
    clock_sheet_reminder_open((uint32_t)(uintptr_t)lv_event_get_user_data(e), clock_app_refresh);
}

static void add_reminder_row(lv_obj_t *list, const clock_reminder_t *r)
{
    void *id = (void *)(uintptr_t)r->id;
    lv_obj_t *row = sui_box_create(list);
    lv_obj_set_size(row, LV_PCT(100), SUI_ROW_H);
    lv_obj_set_style_pad_right(row, 20, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (lv_obj_get_index(row) > 0) {
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(SUI_COLOR_BORDER), 0);
    }

    lv_obj_t *check = sui_box_create(row);
    lv_obj_add_flag(check, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(check, CHECK_TOUCH, CHECK_TOUCH);
    lv_obj_add_event_cb(check, on_reminder_check, LV_EVENT_CLICKED, id);
    lv_obj_t *box = sui_box_create(check);
    lv_obj_set_size(box, CHECK_SIZE, CHECK_SIZE);
    lv_obj_set_style_radius(box, LV_RADIUS_CIRCLE, 0);
    lv_obj_center(box);
    if (r->done) {
        lv_obj_set_style_bg_color(box, lv_color_hex(SUI_COLOR_CAT_CLOCK), 0);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_center(sui_glyph_create(box, SUI_SYMBOL(LV_SYMBOL_OK), 16, SUI_COLOR_ON_ACCENT));
    } else {
        lv_obj_set_style_border_width(box, 2, 0);
        lv_obj_set_style_border_color(box, lv_color_hex(SUI_COLOR_TRACK), 0);
    }

    lv_obj_t *body = sui_box_create(row);
    lv_obj_add_flag(body, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(body, LV_PCT(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_pad_column(body, SUI_SPACE_M, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(body, on_reminder_open, LV_EVENT_CLICKED, id);
    lv_obj_t *text = add_label(body, r->text, SUI_TEXT_ROW, r->done ? SUI_COLOR_TEXT_MUTED : SUI_COLOR_TEXT);
    lv_label_set_long_mode(text, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(text, 1);
    if (r->done)
        lv_obj_set_style_text_decor(text, LV_TEXT_DECOR_STRIKETHROUGH, 0);
    char at[8];
    format_clock_time(r->when, at, sizeof(at));
    add_label(body, r->done ? "Done" : at, SUI_TEXT_BODY, SUI_COLOR_TEXT_MUTED);
    sui_glyph_create(body, SUI_SYMBOL(LV_SYMBOL_RIGHT), 18, SUI_COLOR_TEXT_MUTED);
}

static lv_obj_t *add_group(const char *title)
{
    lv_obj_t *section = sui_box_create(view.reminders);
    lv_obj_set_size(section, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(section, 10, 0);
    lv_obj_set_flex_flow(section, LV_FLEX_FLOW_COLUMN);
    add_section_title(section, title);
    lv_obj_t *list = add_card(section);
    lv_obj_set_size(list, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_clip_corner(list, true, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    return list;
}

static void build_reminders(void)
{
    lv_obj_clean(view.reminders);
    const clock_state_t *s = clock_state();
    const clock_reminder_t *sorted[CLOCK_MAX_REMINDERS];
    size_t count = 0, done = 0;
    for (size_t i = 0; i < s->reminder_count; i++) {
        size_t j = count++;
        while (j > 0 && sorted[j - 1]->when > s->reminders[i].when) {
            sorted[j] = sorted[j - 1];
            j--;
        }
        sorted[j] = &s->reminders[i];
        done += s->reminders[i].done;
    }
    if (!count) {
        add_label(view.reminders, "No reminders yet. Tap Add reminder.", SUI_TEXT_BODY, SUI_COLOR_TEXT_MUTED);
        return;
    }

    int64_t now = (int64_t)time(NULL);
    char group[32] = "", day[32];
    lv_obj_t *list = NULL;
    for (size_t i = 0; i < count; i++) {
        if (sorted[i]->done)
            continue;
        format_day(sorted[i]->when, now, day, sizeof(day));
        if (!list || strcmp(day, group) != 0) {
            snprintf(group, sizeof(group), "%s", day);
            for (char *c = day; *c; c++)
                *c = (char)(*c >= 'a' && *c <= 'z' ? *c - 'a' + 'A' : *c);
            list = add_group(day);
        }
        add_reminder_row(list, sorted[i]);
    }
    if (done) {
        char title[48];
        snprintf(title, sizeof(title), "COMPLETED · %zu", done);
        list = add_group(title);
        for (size_t i = 0; i < count; i++)
            if (sorted[i]->done)
                add_reminder_row(list, sorted[i]);
    }
}

static void create_reminders(lv_obj_t *screen)
{
    view.reminders = view.panes[TAB_REMINDERS] = add_pane(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(view.reminders, 20, 0);
    lv_obj_add_flag(view.reminders, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(view.reminders, LV_DIR_VER);
}

static void on_add(lv_event_t *e)
{
    (void)e;
    if (current_tab == TAB_ALARMS)
        clock_sheet_alarm_open(0, clock_app_refresh);
    else if (current_tab == TAB_REMINDERS)
        clock_sheet_reminder_open(0, clock_app_refresh);
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    if (current_tab == TAB_STOPWATCH)
        update_stopwatch();
    else if (current_tab == TAB_TIMERS)
        update_timer_rows();
}

static void show_tab(size_t tab)
{
    current_tab = tab;
    for (size_t i = 0; i < TAB_COUNT; i++)
        set_visible(view.panes[i], i == tab);
    bool can_add = tab == TAB_ALARMS || tab == TAB_REMINDERS;
    lv_obj_set_style_opa(view.add, can_add ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    if (can_add)
        lv_obj_add_flag(view.add, LV_OBJ_FLAG_CLICKABLE);
    else
        lv_obj_remove_flag(view.add, LV_OBJ_FLAG_CLICKABLE);
    sui_button_set_text(view.add, tab == TAB_REMINDERS ? LV_SYMBOL_PLUS "  Add reminder" : LV_SYMBOL_PLUS "  Add alarm");
    if (view.timer)
        lv_timer_set_period(view.timer, tab == TAB_STOPWATCH ? TICK_FAST_MS : TICK_SLOW_MS / 4);
    refresh_all();
}

static void on_tab(lv_event_t *e)
{
    show_tab(sui_segmented_get(lv_event_get_target(e)));
}

static void refresh_all(void)
{
    if (!view.visible)
        return;
    if (current_tab == TAB_ALARMS)
        build_alarms();
    else if (current_tab == TAB_TIMERS)
        build_timers();
    else if (current_tab == TAB_STOPWATCH) {
        build_laps();
        update_stopwatch();
    } else
        build_reminders();
}

void clock_app_refresh(void)
{
    refresh_all();
}

static void create(lv_obj_t *screen, void *args)
{
    (void)args;
    memset(&view, 0, sizeof(view));
    lv_obj_t *header = sui_header_create(screen, "Clock");
    lv_obj_set_style_pad_right(header, SUI_SPACE_XL, 0);
    lv_obj_t *title = lv_obj_get_child(header, 1);
    lv_obj_set_width(title, 140);

    view.tabs = sui_segmented_create(header, tab_labels, TAB_COUNT, SUI_COLOR_CAT_CLOCK);
    lv_obj_set_width(view.tabs, TABS_W);
    lv_obj_set_style_margin_left(view.tabs, 0, 0);
    lv_obj_add_event_cb(view.tabs, on_tab, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *spacer = sui_box_create(header);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_move_to_index(spacer, 2);
    spacer = sui_box_create(header);
    lv_obj_set_flex_grow(spacer, 1);

    view.add = sui_button_create(header, LV_SYMBOL_PLUS "  Add alarm", SUI_COLOR_CAT_CLOCK, SUI_COLOR_ON_ACCENT);
    lv_obj_set_width(view.add, ADD_W);
    lv_obj_set_style_text_font(view.add, sui_font(SUI_TEXT_BODY), 0);
    lv_obj_add_event_cb(view.add, on_add, LV_EVENT_CLICKED, NULL);

    create_alarms(screen);
    create_timers(screen);
    create_stopwatch(screen);
    create_reminders(screen);
    sui_segmented_set(view.tabs, current_tab);
}

static void on_show(lv_obj_t *screen)
{
    (void)screen;
    view.visible = true;
    char text[TEXT];
    output_text(text, sizeof(text));
    lv_label_set_text(view.output, text);
    view.timer = lv_timer_create(tick, TICK_FAST_MS, NULL);
    show_tab(current_tab);
}

static void on_hide(lv_obj_t *screen)
{
    (void)screen;
    view.visible = false;
    clock_sheet_close();
    if (view.timer)
        lv_timer_delete(view.timer);
    view.timer = NULL;
}

const sui_screen_def_t clock_app_screen = {
    .name = "clock",
    .create = create,
    .on_show = on_show,
    .on_hide = on_hide,
};

void clock_app_select(size_t tab)
{
    if (tab < TAB_COUNT)
        current_tab = tab;
}
