#ifndef SLATE_CLOCK_H
#define SLATE_CLOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CLOCK_MAX_ALARMS 16
#define CLOCK_MAX_REMINDERS 32
#define CLOCK_MAX_TIMERS 8
#define CLOCK_MAX_LAPS 99
#define CLOCK_MAX_EVENTS 16
#define CLOCK_LABEL_MAX 64
#define CLOCK_TEXT_MAX 8192
#define CLOCK_FORMAT_MAX 48
#define CLOCK_MISSED_AFTER 600
#define CLOCK_EVERY_DAY 0x7F
#define CLOCK_WEEKDAYS 0x1F
#define CLOCK_WEEKEND 0x60
#define CLOCK_DEFAULT_SNOOZE 10
#define CLOCK_SYNC_FILE "/run/systemd/timesync/synchronized"

typedef struct {
    uint32_t id;
    uint8_t hour;
    uint8_t minute;
    uint8_t days;
    uint8_t snooze;
    bool on;
    int64_t snoozed_until;
    char label[CLOCK_LABEL_MAX];
} clock_alarm_t;

typedef struct {
    uint32_t id;
    int64_t when;
    bool done;
    bool fired;
    char text[CLOCK_LABEL_MAX];
} clock_reminder_t;

typedef struct {
    uint32_t id;
    int64_t total_ms;
    int64_t left_ms;
    int64_t deadline;
    char label[CLOCK_LABEL_MAX];
} clock_timer_t;

typedef struct {
    bool running;
    int64_t base_ms;
    int64_t since;
    size_t lap_count;
    int64_t laps[CLOCK_MAX_LAPS];
} clock_stopwatch_t;

typedef struct {
    clock_alarm_t alarms[CLOCK_MAX_ALARMS];
    size_t alarm_count;
    clock_reminder_t reminders[CLOCK_MAX_REMINDERS];
    size_t reminder_count;
    clock_timer_t timers[CLOCK_MAX_TIMERS];
    size_t timer_count;
    int64_t last_seen;
    uint32_t next_id;
} clock_state_t;

typedef enum {
    CLOCK_EVENT_ALARM,
    CLOCK_EVENT_ALARM_MISSED,
    CLOCK_EVENT_REMINDER,
    CLOCK_EVENT_REMINDER_MISSED,
    CLOCK_EVENT_TIMER,
} clock_event_kind_t;

typedef struct {
    clock_event_kind_t kind;
    uint32_t id;
    int64_t at;
} clock_event_t;

int clock_weekday(int64_t time);
int64_t clock_alarm_next(const clock_alarm_t *alarm, int64_t after);
size_t clock_collect(clock_state_t *state, bool synced, int64_t now, int64_t mono_ms, clock_event_t *out, size_t max);
int64_t clock_timer_left(const clock_timer_t *timer, int64_t mono_ms);
void clock_timer_pause(clock_timer_t *timer, int64_t mono_ms);
void clock_timer_resume(clock_timer_t *timer, int64_t mono_ms);
void clock_timer_add(clock_timer_t *timer, int64_t ms, int64_t mono_ms);
int64_t clock_stopwatch_elapsed(const clock_stopwatch_t *stopwatch, int64_t mono_ms);
void clock_stopwatch_toggle(clock_stopwatch_t *stopwatch, int64_t mono_ms);
void clock_stopwatch_lap(clock_stopwatch_t *stopwatch, int64_t mono_ms);
void clock_stopwatch_reset(clock_stopwatch_t *stopwatch);

void clock_format_duration(int64_t ms, char *out, size_t len);
void clock_format_stopwatch(int64_t ms, char *main, size_t main_len, char *frac, size_t frac_len);
void clock_format_days(uint8_t days, char *out, size_t len);
void clock_format_until(int64_t seconds, char *out, size_t len);
void clock_state_format(const clock_state_t *state, int64_t now_ms, int64_t mono_ms, char *out, size_t len);
void clock_state_parse(const char *text, int64_t now_ms, int64_t mono_ms, clock_state_t *state);

int64_t clock_now_ms(void);
int64_t clock_mono_ms(void);
bool clock_time_synced(void);
void clock_load(void);
void clock_save(void);
clock_state_t *clock_state(void);
clock_stopwatch_t *clock_stopwatch(void);
uint32_t clock_new_id(void);
clock_alarm_t *clock_find_alarm(uint32_t id);
clock_reminder_t *clock_find_reminder(uint32_t id);
clock_timer_t *clock_find_timer(uint32_t id);
clock_alarm_t *clock_add_alarm(void);
clock_reminder_t *clock_add_reminder(void);
clock_timer_t *clock_add_timer(int64_t ms, const char *label);
void clock_alarm_stop(clock_alarm_t *alarm);
void clock_alarm_snooze(clock_alarm_t *alarm, int64_t now);
void clock_delete_alarm(uint32_t id);
void clock_delete_reminder(uint32_t id);
void clock_delete_timer(uint32_t id);

#endif
