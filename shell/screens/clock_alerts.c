#include "clock_alerts.h"

#include <stdio.h>
#include <time.h>
#include "chime.h"
#include "clock.h"
#include "clock_app.h"
#include "components/button.h"
#include "components/icon.h"
#include "notify.h"
#include "theme/theme.h"

#define TICK_MS 1000
#define SOUND_POLL_MS 500
#define RING_LIMIT_S 600
#define BANNER_LIMIT_S 60
#define QUEUE_MAX 4
#define KEY_MAX 32
#define TEXT 160
#define PULSE_SIZE 360
#define PULSE_MS 1600
#define ICON_TILE 96
#define ICON_GLYPH 52
#define ACTION_W 280
#define ACTION_H 88
#define BANNER_W 720
#define BANNER_TOP 64
#define BANNER_CHIP 52

typedef struct {
    clock_event_kind_t kind;
    uint32_t id;
    int64_t at;
} alert_t;

typedef struct {
    lv_obj_t *overlay;
    alert_t current;
    int64_t shown_at;
    alert_t queue[QUEUE_MAX];
    size_t queued;
    lv_obj_t *banner;
    uint32_t banner_id;
    int64_t banner_at;
} alerts_t;

static alerts_t alerts;

static void show_next(void);

static void make_key(char *out, size_t len, const char *kind, uint32_t id)
{
    snprintf(out, len, "%s:%u", kind, id);
}

void clock_alerts_handled(const char *kind, uint32_t id)
{
    char key[KEY_MAX];
    make_key(key, sizeof(key), kind, id);
    notify_remove(NOTIFY_CLOCK, key);
}

bool clock_alerts_any_alarm_on(void)
{
    const clock_state_t *s = clock_state();
    for (size_t i = 0; i < s->alarm_count; i++)
        if (s->alarms[i].on)
            return true;
    return false;
}

static void format_hm(int64_t when, char *out, size_t len)
{
    time_t t = (time_t)when;
    struct tm local;
    localtime_r(&t, &local);
    strftime(out, len, "%H:%M", &local);
}

static void timer_name(const clock_timer_t *t, char *out, size_t len)
{
    char total[CLOCK_FORMAT_MAX];
    clock_format_duration(t->total_ms, total, sizeof(total));
    if (t->label[0])
        snprintf(out, len, "%s · %s timer", t->label, total);
    else
        snprintf(out, len, "%s timer", total);
}

static void post_missed_alarm(const clock_alarm_t *a)
{
    char key[KEY_MAX], body[TEXT];
    make_key(key, sizeof(key), CLOCK_KEY_ALARM, a->id);
    snprintf(body, sizeof(body), "%02u:%02u%s%s", a->hour, a->minute, a->label[0] ? " · " : "", a->label);
    notify_post(NOTIFY_CLOCK, NOTIFY_ICON_ALARM, key, "Missed alarm", body);
}

static void post_reminder(const clock_reminder_t *r)
{
    char key[KEY_MAX], at[8], body[TEXT];
    make_key(key, sizeof(key), CLOCK_KEY_REMINDER, r->id);
    format_hm(r->when, at, sizeof(at));
    snprintf(body, sizeof(body), "Reminder for %s", at);
    notify_post(NOTIFY_CLOCK, NOTIFY_ICON_REMINDER, key, r->text, body);
}

static void post_timer(const clock_timer_t *t)
{
    char key[KEY_MAX], body[TEXT];
    make_key(key, sizeof(key), CLOCK_KEY_TIMER, t->id);
    timer_name(t, body, sizeof(body));
    notify_post(NOTIFY_CLOCK, NOTIFY_ICON_TIMER, key, "Timer done", body);
}

static void wake_screen(void)
{
    lv_display_trigger_activity(NULL);
}

static void close_overlay(void)
{
    chime_stop();
    if (alerts.overlay)
        lv_obj_delete_async(alerts.overlay);
    alerts.overlay = NULL;
    clock_save();
    clock_app_refresh();
    show_next();
}

static void on_secondary(lv_event_t *e)
{
    (void)e;
    if (alerts.current.kind == CLOCK_EVENT_ALARM) {
        clock_alarm_t *a = clock_find_alarm(alerts.current.id);
        if (a)
            clock_alarm_snooze(a, (int64_t)time(NULL));
    } else {
        clock_timer_t *t = clock_find_timer(alerts.current.id);
        if (t)
            clock_timer_add(t, 60000, clock_mono_ms());
        clock_alerts_handled(CLOCK_KEY_TIMER, alerts.current.id);
    }
    close_overlay();
}

static void on_stop(lv_event_t *e)
{
    (void)e;
    if (alerts.current.kind == CLOCK_EVENT_ALARM) {
        clock_alarm_t *a = clock_find_alarm(alerts.current.id);
        if (a)
            clock_alarm_stop(a);
    } else {
        clock_alerts_handled(CLOCK_KEY_TIMER, alerts.current.id);
        clock_delete_timer(alerts.current.id);
    }
    close_overlay();
}

static lv_obj_t *add_label(lv_obj_t *parent, const char *text, sui_text_t font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, sui_font(font), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

static lv_obj_t *add_action(lv_obj_t *parent, const char *text, uint32_t fill, uint32_t ink, lv_event_cb_t cb)
{
    lv_obj_t *button = sui_button_create(parent, text, fill, ink);
    lv_obj_set_size(button, ACTION_W, ACTION_H);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_text_font(button, sui_font(SUI_TEXT_HEADING), 0);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);
    return button;
}

static void pulse(void *obj, int32_t value)
{
    lv_obj_set_style_bg_opa(obj, (lv_opa_t)(20 + value * 70 / 100), 0);
    lv_obj_set_style_transform_scale(obj, 256 + value * 40 / 100, 0);
}

static void build_overlay(const char *kicker, const char *big, const char *sub, const char *secondary,
                          sui_glyph_t glyph)
{
    lv_obj_t *overlay = alerts.overlay = sui_box_create(lv_layer_top());
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, lv_color_hex(SUI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(overlay, lv_color_hex(SUI_COLOR_TEXT), 0);
    lv_obj_set_style_pad_row(overlay, 28, 0);
    lv_obj_set_flex_flow(overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *ring = sui_box_create(overlay);
    lv_obj_add_flag(ring, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(ring, PULSE_SIZE, PULSE_SIZE);
    lv_obj_align(ring, LV_ALIGN_CENTER, 0, -90);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ring, lv_color_hex(SUI_COLOR_CAT_CLOCK), 0);
    lv_obj_set_style_transform_pivot_x(ring, PULSE_SIZE / 2, 0);
    lv_obj_set_style_transform_pivot_y(ring, PULSE_SIZE / 2, 0);
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, ring);
    lv_anim_set_values(&anim, 0, 100);
    lv_anim_set_duration(&anim, PULSE_MS / 2);
    lv_anim_set_playback_duration(&anim, PULSE_MS / 2);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&anim, pulse);
    lv_anim_start(&anim);

    lv_obj_t *tile = sui_box_create(overlay);
    lv_obj_set_size(tile, ICON_TILE, ICON_TILE);
    lv_obj_set_style_radius(tile, 30, 0);
    lv_obj_set_style_bg_color(tile, lv_color_hex(SUI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_center(sui_glyph_create(tile, glyph, ICON_GLYPH, SUI_COLOR_CAT_CLOCK));

    lv_obj_t *texts = sui_box_create(overlay);
    lv_obj_set_style_pad_row(texts, SUI_SPACE_S, 0);
    lv_obj_set_flex_flow(texts, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(texts, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *label = add_label(texts, kicker, SUI_TEXT_BODY, SUI_COLOR_CAT_CLOCK);
    lv_obj_set_style_text_letter_space(label, 2, 0);
    label = add_label(texts, big, SUI_TEXT_DISPLAY, SUI_COLOR_TEXT);
    lv_obj_set_style_text_font(label, sui_font_digits(), 0);
    add_label(texts, sub, SUI_TEXT_HEADING, SUI_COLOR_TEXT_MUTED);

    lv_obj_t *actions = sui_box_create(overlay);
    lv_obj_set_style_pad_top(actions, SUI_SPACE_L, 0);
    lv_obj_set_style_pad_column(actions, SUI_SPACE_L, 0);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    if (secondary)
        add_action(actions, secondary, SUI_COLOR_RAISED, SUI_COLOR_TEXT, on_secondary);
    add_action(actions, "Stop", SUI_COLOR_CAT_CLOCK, SUI_COLOR_ON_ACCENT, on_stop);
}

static bool show_alert(const alert_t *alert)
{
    char big[16], sub[TEXT], secondary[32];
    if (alert->kind == CLOCK_EVENT_ALARM) {
        const clock_alarm_t *a = clock_find_alarm(alert->id);
        if (!a)
            return false;
        char day[16];
        time_t t = (time_t)alert->at;
        struct tm local;
        localtime_r(&t, &local);
        strftime(day, sizeof(day), "%A", &local);
        snprintf(big, sizeof(big), "%02u:%02u", a->hour, a->minute);
        snprintf(sub, sizeof(sub), "%s%s%s", a->label, a->label[0] ? " · " : "", day);
        snprintf(secondary, sizeof(secondary), "Snooze %u min", a->snooze);
        build_overlay("ALARM", big, sub, a->snooze ? secondary : NULL, SUI_VECTOR(SUI_ICON_ALARM));
        chime_start(CHIME_ALARM);
    } else {
        const clock_timer_t *t = clock_find_timer(alert->id);
        if (!t)
            return false;
        timer_name(t, sub, sizeof(sub));
        build_overlay("TIMER DONE", "0:00", sub, "+1 min", SUI_VECTOR(SUI_ICON_TIMER));
        chime_start(CHIME_TIMER);
    }
    alerts.current = *alert;
    alerts.shown_at = (int64_t)time(NULL);
    wake_screen();
    return true;
}

static void show_next(void)
{
    while (!alerts.overlay && alerts.queued) {
        alert_t next = alerts.queue[0];
        alerts.queued--;
        for (size_t i = 0; i < alerts.queued; i++)
            alerts.queue[i] = alerts.queue[i + 1];
        show_alert(&next);
    }
}

static void queue_alert(clock_event_kind_t kind, uint32_t id, int64_t at)
{
    if (alerts.queued < QUEUE_MAX)
        alerts.queue[alerts.queued++] = (alert_t){ .kind = kind, .id = id, .at = at };
    show_next();
}

static void hide_banner(void)
{
    if (alerts.banner)
        lv_obj_delete_async(alerts.banner);
    alerts.banner = NULL;
}

static void on_banner_later(lv_event_t *e)
{
    (void)e;
    clock_reminder_t *r = clock_find_reminder(alerts.banner_id);
    if (r) {
        r->when = (int64_t)time(NULL) + 3600;
        r->fired = false;
        clock_alerts_handled(CLOCK_KEY_REMINDER, r->id);
        clock_save();
    }
    hide_banner();
    clock_app_refresh();
}

static void on_banner_done(lv_event_t *e)
{
    (void)e;
    clock_reminder_t *r = clock_find_reminder(alerts.banner_id);
    if (r) {
        r->done = true;
        clock_alerts_handled(CLOCK_KEY_REMINDER, r->id);
        clock_save();
    }
    hide_banner();
    clock_app_refresh();
}

static void show_banner(const clock_reminder_t *r)
{
    hide_banner();
    lv_obj_t *banner = alerts.banner = sui_box_create(lv_layer_top());
    lv_obj_add_flag(banner, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(banner, BANNER_W, LV_SIZE_CONTENT);
    lv_obj_align(banner, LV_ALIGN_TOP_MID, 0, BANNER_TOP);
    lv_obj_set_style_pad_all(banner, SUI_SPACE_M, 0);
    lv_obj_set_style_pad_left(banner, 20, 0);
    lv_obj_set_style_pad_column(banner, SUI_SPACE_M, 0);
    lv_obj_set_style_radius(banner, 22, 0);
    lv_obj_set_style_bg_color(banner, lv_color_hex(SUI_COLOR_RAISED), 0);
    lv_obj_set_style_bg_opa(banner, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(banner, lv_color_hex(SUI_COLOR_TRACK), 0);
    lv_obj_set_style_border_width(banner, 1, 0);
    lv_obj_set_flex_flow(banner, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(banner, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *chip = sui_box_create(banner);
    lv_obj_set_size(chip, BANNER_CHIP, BANNER_CHIP);
    lv_obj_set_style_radius(chip, 16, 0);
    lv_obj_set_style_bg_color(chip, lv_color_hex(SUI_COLOR_CAT_CLOCK), 0);
    lv_obj_set_style_bg_opa(chip, SUI_TINT_OPA, 0);
    lv_obj_center(sui_glyph_create(chip, SUI_VECTOR(SUI_ICON_REMINDER), 26, SUI_COLOR_CAT_CLOCK));

    lv_obj_t *texts = sui_box_create(banner);
    lv_obj_set_height(texts, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(texts, 1);
    lv_obj_set_style_pad_row(texts, SUI_SPACE_XS, 0);
    lv_obj_set_flex_flow(texts, LV_FLEX_FLOW_COLUMN);
    char at[8], meta[32];
    format_hm(r->when, at, sizeof(at));
    snprintf(meta, sizeof(meta), "Reminder · %s", at);
    add_label(texts, meta, SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);
    lv_obj_t *title = add_label(texts, r->text, SUI_TEXT_ROW, SUI_COLOR_TEXT);
    lv_obj_set_width(title, LV_PCT(100));
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);

    lv_obj_t *later = sui_button_create(banner, "In 1 hour", SUI_COLOR_BORDER, SUI_COLOR_TEXT);
    lv_obj_add_event_cb(later, on_banner_later, LV_EVENT_CLICKED, NULL);
    lv_obj_t *done = sui_button_create(banner, "Done", SUI_COLOR_CAT_CLOCK, SUI_COLOR_ON_ACCENT);
    lv_obj_add_event_cb(done, on_banner_done, LV_EVENT_CLICKED, NULL);
    lv_obj_move_foreground(banner);

    alerts.banner_id = r->id;
    alerts.banner_at = (int64_t)time(NULL);
}

static void handle(const clock_event_t *event)
{
    switch (event->kind) {
    case CLOCK_EVENT_ALARM:
        queue_alert(event->kind, event->id, event->at);
        break;
    case CLOCK_EVENT_ALARM_MISSED: {
        const clock_alarm_t *a = clock_find_alarm(event->id);
        if (a)
            post_missed_alarm(a);
        break;
    }
    case CLOCK_EVENT_REMINDER:
    case CLOCK_EVENT_REMINDER_MISSED: {
        const clock_reminder_t *r = clock_find_reminder(event->id);
        if (!r)
            break;
        post_reminder(r);
        if (event->kind == CLOCK_EVENT_REMINDER) {
            show_banner(r);
            if (!alerts.overlay)
                chime_start(CHIME_REMINDER);
            wake_screen();
        }
        break;
    }
    case CLOCK_EVENT_TIMER: {
        const clock_timer_t *t = clock_find_timer(event->id);
        if (t)
            post_timer(t);
        queue_alert(event->kind, event->id, event->at);
        break;
    }
    }
}

static void expire(int64_t now)
{
    if (alerts.overlay && now - alerts.shown_at >= RING_LIMIT_S) {
        if (alerts.current.kind == CLOCK_EVENT_ALARM) {
            clock_alarm_t *a = clock_find_alarm(alerts.current.id);
            if (a) {
                post_missed_alarm(a);
                clock_alarm_stop(a);
            }
        }
        close_overlay();
    }
    if (alerts.banner && now - alerts.banner_at >= BANNER_LIMIT_S)
        hide_banner();
    if (alerts.overlay)
        wake_screen();
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    clock_event_t events[CLOCK_MAX_EVENTS];
    int64_t now = (int64_t)time(NULL);
    size_t count = clock_collect(clock_state(), clock_time_synced(), now, clock_mono_ms(), events, CLOCK_MAX_EVENTS);
    if (count) {
        clock_save();
        for (size_t i = 0; i < count; i++)
            handle(&events[i]);
        clock_app_refresh();
    }
    expire(now);
}

static void poll_sound(lv_timer_t *timer)
{
    (void)timer;
    chime_poll();
}

void clock_alerts_start(void)
{
    clock_load();
    tick(lv_timer_create(tick, TICK_MS, NULL));
    lv_timer_create(poll_sound, SOUND_POLL_MS, NULL);
}
