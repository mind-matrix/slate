#include "clock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "sysinfo.h"

#define CONFIG_DIR ".config/slate"
#define CONFIG_FILE "clock"
#define PATH_LEN 256
#define MAX_FIELDS 9

static clock_state_t state = { .next_id = 1 };
static clock_stopwatch_t stopwatch;

static const char *const day_names[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };

int clock_weekday(int64_t time)
{
    time_t t = (time_t)time;
    struct tm local;
    localtime_r(&t, &local);
    return (local.tm_wday + 6) % 7;
}

int64_t clock_alarm_next(const clock_alarm_t *alarm, int64_t after)
{
    time_t base = (time_t)after;
    struct tm day;
    localtime_r(&base, &day);
    int64_t next = 0;
    for (int k = 0; k <= 7 && !next; k++) {
        struct tm at = day;
        at.tm_mday += k;
        at.tm_hour = alarm->hour;
        at.tm_min = alarm->minute;
        at.tm_sec = 0;
        at.tm_isdst = -1;
        int64_t t = (int64_t)mktime(&at);
        if (t > after && (alarm->days == 0 || (alarm->days >> clock_weekday(t) & 1)))
            next = t;
    }
    if (alarm->snoozed_until > after && alarm->snoozed_until < next)
        next = alarm->snoozed_until;
    return next;
}

static size_t push(clock_event_t *out, size_t count, size_t max, clock_event_kind_t kind, uint32_t id, int64_t at)
{
    if (count < max)
        out[count++] = (clock_event_t){ .kind = kind, .id = id, .at = at };
    return count;
}

size_t clock_collect(clock_state_t *s, bool synced, int64_t now, int64_t mono_ms, clock_event_t *out, size_t max)
{
    size_t count = 0;
    for (size_t i = 0; i < s->timer_count; i++) {
        clock_timer_t *t = &s->timers[i];
        if (!t->deadline || t->deadline > mono_ms)
            continue;
        count = push(out, count, max, CLOCK_EVENT_TIMER, t->id, now);
        t->left_ms = 0;
        t->deadline = 0;
    }
    if (!synced)
        return count;
    int64_t since = s->last_seen ? s->last_seen : now;
    for (size_t i = 0; i < s->alarm_count; i++) {
        clock_alarm_t *a = &s->alarms[i];
        if (!a->on)
            continue;
        int64_t at = clock_alarm_next(a, since);
        if (!at || at > now)
            continue;
        bool missed = now - at > CLOCK_MISSED_AFTER;
        count = push(out, count, max, missed ? CLOCK_EVENT_ALARM_MISSED : CLOCK_EVENT_ALARM, a->id, at);
        if (a->snoozed_until && a->snoozed_until <= now)
            a->snoozed_until = 0;
        if (missed && a->days == 0)
            a->on = false;
    }
    for (size_t i = 0; i < s->reminder_count; i++) {
        clock_reminder_t *r = &s->reminders[i];
        if (r->done || r->fired || r->when > now)
            continue;
        bool missed = now - r->when > CLOCK_MISSED_AFTER;
        count = push(out, count, max, missed ? CLOCK_EVENT_REMINDER_MISSED : CLOCK_EVENT_REMINDER, r->id, r->when);
        r->fired = true;
    }
    s->last_seen = now;
    return count;
}

int64_t clock_timer_left(const clock_timer_t *timer, int64_t mono_ms)
{
    if (!timer->deadline)
        return timer->left_ms;
    return timer->deadline > mono_ms ? timer->deadline - mono_ms : 0;
}

void clock_timer_pause(clock_timer_t *timer, int64_t mono_ms)
{
    timer->left_ms = clock_timer_left(timer, mono_ms);
    timer->deadline = 0;
}

void clock_timer_resume(clock_timer_t *timer, int64_t mono_ms)
{
    if (!timer->deadline && timer->left_ms > 0)
        timer->deadline = mono_ms + timer->left_ms;
}

void clock_timer_add(clock_timer_t *timer, int64_t ms, int64_t mono_ms)
{
    bool running = timer->deadline || timer->left_ms == 0;
    timer->left_ms = clock_timer_left(timer, mono_ms) + ms;
    timer->total_ms += ms;
    timer->deadline = running ? mono_ms + timer->left_ms : 0;
}

int64_t clock_stopwatch_elapsed(const clock_stopwatch_t *sw, int64_t mono_ms)
{
    return sw->base_ms + (sw->running ? mono_ms - sw->since : 0);
}

void clock_stopwatch_toggle(clock_stopwatch_t *sw, int64_t mono_ms)
{
    if (sw->running)
        sw->base_ms = clock_stopwatch_elapsed(sw, mono_ms);
    else
        sw->since = mono_ms;
    sw->running = !sw->running;
}

void clock_stopwatch_lap(clock_stopwatch_t *sw, int64_t mono_ms)
{
    if (sw->running && sw->lap_count < CLOCK_MAX_LAPS)
        sw->laps[sw->lap_count++] = clock_stopwatch_elapsed(sw, mono_ms);
}

void clock_stopwatch_reset(clock_stopwatch_t *sw)
{
    *sw = (clock_stopwatch_t){ 0 };
}

void clock_format_duration(int64_t ms, char *out, size_t len)
{
    int64_t total = (ms + 999) / 1000;
    int h = (int)(total / 3600), m = (int)(total / 60 % 60), s = (int)(total % 60);
    if (h)
        snprintf(out, len, "%d:%02d:%02d", h, m, s);
    else
        snprintf(out, len, "%d:%02d", m, s);
}

void clock_format_stopwatch(int64_t ms, char *main, size_t main_len, char *frac, size_t frac_len)
{
    int h = (int)(ms / 3600000), m = (int)(ms / 60000 % 60), s = (int)(ms / 1000 % 60);
    if (h)
        snprintf(main, main_len, "%d:%02d:%02d", h, m, s);
    else
        snprintf(main, main_len, "%02d:%02d", m, s);
    snprintf(frac, frac_len, ".%02d", (int)(ms / 10 % 100));
}

void clock_format_days(uint8_t days, char *out, size_t len)
{
    days &= CLOCK_EVERY_DAY;
    if (days == 0)
        snprintf(out, len, "Once");
    else if (days == CLOCK_EVERY_DAY)
        snprintf(out, len, "Every day");
    else if (days == CLOCK_WEEKDAYS)
        snprintf(out, len, "Mon–Fri");
    else if (days == CLOCK_WEEKEND)
        snprintf(out, len, "Sat, Sun");
    else {
        size_t used = 0;
        out[0] = '\0';
        for (int i = 0; i < 7; i++)
            if (days >> i & 1 && used < len)
                used += (size_t)snprintf(out + used, len - used, "%s%s", used ? ", " : "", day_names[i]);
    }
}

void clock_format_until(int64_t seconds, char *out, size_t len)
{
    int64_t minutes = (seconds + 59) / 60;
    int d = (int)(minutes / 1440), h = (int)(minutes / 60 % 24), m = (int)(minutes % 60);
    if (d)
        snprintf(out, len, "%d d %d h", d, h);
    else if (h)
        snprintf(out, len, "%d h %d min", h, m);
    else
        snprintf(out, len, "%d min", m);
}

static void clean(char *out, size_t len, const char *text)
{
    size_t i = 0;
    for (; text[i] && i + 1 < len; i++)
        out[i] = (text[i] == '\t' || text[i] == '\n') ? ' ' : text[i];
    out[i] = '\0';
}

static size_t append(char *out, size_t len, size_t used, const char *line)
{
    size_t n = strlen(line);
    if (used + n < len) {
        memcpy(out + used, line, n + 1);
        used += n;
    }
    return used;
}

void clock_state_format(const clock_state_t *s, int64_t now_ms, int64_t mono_ms, char *out, size_t len)
{
    char line[256], label[CLOCK_LABEL_MAX];
    size_t used = 0;
    if (len)
        out[0] = '\0';
    snprintf(line, sizeof(line), "next\t%u\nseen\t%lld\n", s->next_id, (long long)s->last_seen);
    used = append(out, len, used, line);
    for (size_t i = 0; i < s->alarm_count; i++) {
        const clock_alarm_t *a = &s->alarms[i];
        clean(label, sizeof(label), a->label);
        snprintf(line, sizeof(line), "alarm\t%u\t%u\t%u\t%u\t%d\t%u\t%lld\t%s\n", a->id, a->hour, a->minute, a->days,
                 a->on ? 1 : 0, a->snooze, (long long)a->snoozed_until, label);
        used = append(out, len, used, line);
    }
    for (size_t i = 0; i < s->reminder_count; i++) {
        const clock_reminder_t *r = &s->reminders[i];
        clean(label, sizeof(label), r->text);
        snprintf(line, sizeof(line), "reminder\t%u\t%lld\t%d\t%d\t%s\n", r->id, (long long)r->when, r->done ? 1 : 0,
                 r->fired ? 1 : 0, label);
        used = append(out, len, used, line);
    }
    for (size_t i = 0; i < s->timer_count; i++) {
        const clock_timer_t *t = &s->timers[i];
        int64_t ends_at = t->deadline ? now_ms + (t->deadline - mono_ms) : 0;
        clean(label, sizeof(label), t->label);
        snprintf(line, sizeof(line), "timer\t%u\t%lld\t%lld\t%lld\t%s\n", t->id, (long long)t->total_ms,
                 (long long)clock_timer_left(t, mono_ms), (long long)ends_at, label);
        used = append(out, len, used, line);
    }
}

static size_t split(char *line, char **fields)
{
    size_t count = 0;
    for (char *p = line; p && count < MAX_FIELDS; count++) {
        fields[count] = p;
        p = strchr(p, '\t');
        if (p)
            *p++ = '\0';
    }
    return count;
}

static long long number(const char *text)
{
    return strtoll(text, NULL, 10);
}

void clock_state_parse(const char *text, int64_t now_ms, int64_t mono_ms, clock_state_t *s)
{
    *s = (clock_state_t){ .next_id = 1 };
    char line[256];
    char *fields[MAX_FIELDS];
    for (const char *p = text; p && *p;) {
        const char *next = strchr(p, '\n');
        size_t n = next ? (size_t)(next - p) : strlen(p);
        if (n >= sizeof(line))
            n = sizeof(line) - 1;
        memcpy(line, p, n);
        line[n] = '\0';
        p = next ? next + 1 : NULL;

        size_t count = split(line, fields);
        if (count == 2 && strcmp(fields[0], "next") == 0)
            s->next_id = (uint32_t)number(fields[1]);
        else if (count == 2 && strcmp(fields[0], "seen") == 0)
            s->last_seen = number(fields[1]);
        else if (count == 9 && strcmp(fields[0], "alarm") == 0 && s->alarm_count < CLOCK_MAX_ALARMS) {
            long long hour = number(fields[2]), minute = number(fields[3]);
            if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
                continue;
            clock_alarm_t *a = &s->alarms[s->alarm_count++];
            a->id = (uint32_t)number(fields[1]);
            a->hour = (uint8_t)hour;
            a->minute = (uint8_t)minute;
            a->days = (uint8_t)(number(fields[4]) & CLOCK_EVERY_DAY);
            a->on = number(fields[5]) != 0;
            a->snooze = (uint8_t)number(fields[6]);
            a->snoozed_until = number(fields[7]);
            snprintf(a->label, sizeof(a->label), "%s", fields[8]);
        } else if (count == 6 && strcmp(fields[0], "reminder") == 0 && s->reminder_count < CLOCK_MAX_REMINDERS) {
            clock_reminder_t *r = &s->reminders[s->reminder_count++];
            r->id = (uint32_t)number(fields[1]);
            r->when = number(fields[2]);
            r->done = number(fields[3]) != 0;
            r->fired = number(fields[4]) != 0;
            snprintf(r->text, sizeof(r->text), "%s", fields[5]);
        } else if (count == 6 && strcmp(fields[0], "timer") == 0 && s->timer_count < CLOCK_MAX_TIMERS) {
            clock_timer_t *t = &s->timers[s->timer_count++];
            long long ends_at = number(fields[4]);
            t->id = (uint32_t)number(fields[1]);
            t->total_ms = number(fields[2]);
            t->left_ms = number(fields[3]);
            t->deadline = ends_at ? mono_ms + (ends_at > now_ms ? ends_at - now_ms : 0) : 0;
            snprintf(t->label, sizeof(t->label), "%s", fields[5]);
        }
    }
}

int64_t clock_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int64_t clock_mono_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static bool config_path(char *path, size_t len, bool create_dir)
{
    const char *home = getenv("HOME");
    if (!home || !*home)
        return false;
    if (create_dir) {
        char dir[PATH_LEN];
        snprintf(dir, sizeof(dir), "%s/.config", home);
        mkdir(dir, 0755);
        snprintf(dir, sizeof(dir), "%s/%s", home, CONFIG_DIR);
        mkdir(dir, 0755);
    }
    return snprintf(path, len, "%s/%s/%s", home, CONFIG_DIR, CONFIG_FILE) < (int)len;
}

void clock_load(void)
{
    static char text[CLOCK_TEXT_MAX];
    char path[PATH_LEN];
    if (config_path(path, sizeof(path), false) && sysinfo_read_file(path, text, sizeof(text)))
        clock_state_parse(text, clock_now_ms(), clock_mono_ms(), &state);
}

void clock_save(void)
{
    static char text[CLOCK_TEXT_MAX];
    char path[PATH_LEN], tmp[PATH_LEN + 8];
    if (!config_path(path, sizeof(path), true))
        return;
    clock_state_format(&state, clock_now_ms(), clock_mono_ms(), text, sizeof(text));
    snprintf(tmp, sizeof(tmp), "%s.new", path);
    FILE *file = fopen(tmp, "w");
    if (!file)
        return;
    bool written = fputs(text, file) >= 0;
    written = fflush(file) == 0 && written;
    if (fclose(file) == 0 && written)
        rename(tmp, path);
}

clock_state_t *clock_state(void)
{
    return &state;
}

clock_stopwatch_t *clock_stopwatch(void)
{
    return &stopwatch;
}

uint32_t clock_new_id(void)
{
    return state.next_id++;
}

clock_alarm_t *clock_find_alarm(uint32_t id)
{
    for (size_t i = 0; i < state.alarm_count; i++)
        if (state.alarms[i].id == id)
            return &state.alarms[i];
    return NULL;
}

clock_reminder_t *clock_find_reminder(uint32_t id)
{
    for (size_t i = 0; i < state.reminder_count; i++)
        if (state.reminders[i].id == id)
            return &state.reminders[i];
    return NULL;
}

clock_timer_t *clock_find_timer(uint32_t id)
{
    for (size_t i = 0; i < state.timer_count; i++)
        if (state.timers[i].id == id)
            return &state.timers[i];
    return NULL;
}

clock_alarm_t *clock_add_alarm(void)
{
    if (state.alarm_count == CLOCK_MAX_ALARMS)
        return NULL;
    clock_alarm_t *a = &state.alarms[state.alarm_count++];
    *a = (clock_alarm_t){ .id = clock_new_id(), .hour = 7, .days = CLOCK_WEEKDAYS, .snooze = CLOCK_DEFAULT_SNOOZE,
                          .on = true };
    return a;
}

clock_reminder_t *clock_add_reminder(void)
{
    if (state.reminder_count == CLOCK_MAX_REMINDERS)
        return NULL;
    clock_reminder_t *r = &state.reminders[state.reminder_count++];
    *r = (clock_reminder_t){ .id = clock_new_id() };
    return r;
}

clock_timer_t *clock_add_timer(int64_t ms, const char *label)
{
    if (state.timer_count == CLOCK_MAX_TIMERS || ms <= 0)
        return NULL;
    memmove(&state.timers[1], &state.timers[0], state.timer_count * sizeof(clock_timer_t));
    state.timer_count++;
    clock_timer_t *t = &state.timers[0];
    *t = (clock_timer_t){ .id = clock_new_id(), .total_ms = ms, .left_ms = ms, .deadline = clock_mono_ms() + ms };
    snprintf(t->label, sizeof(t->label), "%s", label ? label : "");
    return t;
}

#define DELETE_BY_ID(array, count, id)                                                   \
    do {                                                                                 \
        for (size_t i = 0; i < (count); i++) {                                           \
            if ((array)[i].id == (id)) {                                                 \
                memmove(&(array)[i], &(array)[i + 1], ((count) - i - 1) * sizeof((array)[0])); \
                (count)--;                                                               \
                break;                                                                   \
            }                                                                            \
        }                                                                                \
    } while (0)

void clock_alarm_stop(clock_alarm_t *alarm)
{
    alarm->snoozed_until = 0;
    if (alarm->days == 0)
        alarm->on = false;
}

void clock_alarm_snooze(clock_alarm_t *alarm, int64_t now)
{
    alarm->snoozed_until = now + (int64_t)(alarm->snooze ? alarm->snooze : CLOCK_DEFAULT_SNOOZE) * 60;
}

void clock_delete_alarm(uint32_t id)
{
    DELETE_BY_ID(state.alarms, state.alarm_count, id);
}

void clock_delete_reminder(uint32_t id)
{
    DELETE_BY_ID(state.reminders, state.reminder_count, id);
}

void clock_delete_timer(uint32_t id)
{
    DELETE_BY_ID(state.timers, state.timer_count, id);
}
