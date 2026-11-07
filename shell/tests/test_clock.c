#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "check.h"
#include "clock.h"

#define SAT_1210 1794053400
#define HOUR 3600
#define DAY 86400

static void use_tz(const char *tz)
{
    setenv("TZ", tz, 1);
    tzset();
}

static clock_alarm_t alarm_at(int hour, int minute, uint8_t days)
{
    return (clock_alarm_t){ .id = 1, .hour = (uint8_t)hour, .minute = (uint8_t)minute, .days = days, .on = true };
}

static void test_next_alarm(void)
{
    use_tz("UTC");
    CHECK(clock_weekday(SAT_1210) == 5);
    clock_alarm_t weekdays = alarm_at(7, 0, CLOCK_WEEKDAYS);
    CHECK(clock_alarm_next(&weekdays, SAT_1210) == SAT_1210 - 43800 + 2 * DAY + 7 * HOUR);
    clock_alarm_t once = alarm_at(13, 0, 0);
    CHECK(clock_alarm_next(&once, SAT_1210) == SAT_1210 + 50 * 60);
    clock_alarm_t now = alarm_at(12, 10, 0);
    CHECK(clock_alarm_next(&now, SAT_1210) == SAT_1210 + DAY);
    clock_alarm_t sunday = alarm_at(9, 0, 1 << 6);
    CHECK(clock_alarm_next(&sunday, SAT_1210) == SAT_1210 - 43800 + DAY + 9 * HOUR);
    weekdays.snoozed_until = SAT_1210 + 600;
    CHECK(clock_alarm_next(&weekdays, SAT_1210) == SAT_1210 + 600);
}

static void test_next_alarm_dst(void)
{
    use_tz("GMT0BST,M3.5.0/1,M10.5.0");
    clock_alarm_t gap = alarm_at(1, 30, CLOCK_EVERY_DAY);
    CHECK(clock_alarm_next(&gap, 1774699200) == 1774747800);
    clock_alarm_t after_fall_back = alarm_at(7, 0, CLOCK_EVERY_DAY);
    CHECK(clock_alarm_next(&after_fall_back, 1792843200) == 1792911600);
    use_tz("UTC");
}

static void test_collect_alarms(void)
{
    use_tz("UTC");
    clock_state_t s = { .alarm_count = 2, .last_seen = SAT_1210 };
    s.alarms[0] = alarm_at(12, 15, CLOCK_EVERY_DAY);
    s.alarms[1] = alarm_at(12, 20, 0);
    s.alarms[1].id = 2;
    clock_event_t events[CLOCK_MAX_EVENTS];

    CHECK(clock_collect(&s, true, SAT_1210 + 200, 0, events, CLOCK_MAX_EVENTS) == 0);
    CHECK(clock_collect(&s, true, SAT_1210 + 330, 0, events, CLOCK_MAX_EVENTS) == 1);
    CHECK(events[0].kind == CLOCK_EVENT_ALARM && events[0].id == 1 && events[0].at == SAT_1210 + 300);
    CHECK(clock_collect(&s, true, SAT_1210 + 360, 0, events, CLOCK_MAX_EVENTS) == 0);

    CHECK(clock_collect(&s, true, SAT_1210 + 600 + 700, 0, events, CLOCK_MAX_EVENTS) == 1);
    CHECK(events[0].kind == CLOCK_EVENT_ALARM_MISSED && events[0].id == 2);
    CHECK(!s.alarms[1].on);

    clock_alarm_t ringing = alarm_at(13, 0, 0);
    clock_alarm_snooze(&ringing, SAT_1210);
    CHECK(ringing.snoozed_until == SAT_1210 + CLOCK_DEFAULT_SNOOZE * 60);
    clock_alarm_stop(&ringing);
    CHECK(!ringing.on && ringing.snoozed_until == 0);
}

static void test_collect_first_run(void)
{
    clock_state_t s = { .alarm_count = 1 };
    s.alarms[0] = alarm_at(12, 0, CLOCK_EVERY_DAY);
    clock_event_t events[CLOCK_MAX_EVENTS];
    CHECK(clock_collect(&s, true, SAT_1210, 0, events, CLOCK_MAX_EVENTS) == 0);
    CHECK(s.last_seen == SAT_1210);
}

static void test_collect_reminders_and_timers(void)
{
    clock_state_t s = { .reminder_count = 3, .timer_count = 2, .last_seen = SAT_1210 };
    s.reminders[0] = (clock_reminder_t){ .id = 10, .when = SAT_1210 + 60 };
    s.reminders[1] = (clock_reminder_t){ .id = 11, .when = SAT_1210 - 3 * HOUR };
    s.reminders[2] = (clock_reminder_t){ .id = 12, .when = SAT_1210 - 60, .done = true };
    s.timers[0] = (clock_timer_t){ .id = 20, .total_ms = 5000, .left_ms = 5000, .deadline = 5000 };
    s.timers[1] = (clock_timer_t){ .id = 21, .total_ms = 9000, .left_ms = 9000 };
    clock_event_t events[CLOCK_MAX_EVENTS];

    size_t n = clock_collect(&s, true, SAT_1210 + 61, 5000, events, CLOCK_MAX_EVENTS);
    CHECK(n == 3);
    CHECK(events[0].kind == CLOCK_EVENT_TIMER && events[0].id == 20);
    CHECK(events[1].kind == CLOCK_EVENT_REMINDER && events[1].id == 10);
    CHECK(events[2].kind == CLOCK_EVENT_REMINDER_MISSED && events[2].id == 11);
    CHECK(s.reminders[0].fired && s.reminders[1].fired && !s.reminders[2].fired);
    CHECK(s.timers[0].left_ms == 0 && s.timers[0].deadline == 0);
    CHECK(clock_collect(&s, true, SAT_1210 + 62, 6000, events, CLOCK_MAX_EVENTS) == 0);
}

static void test_collect_unsynced(void)
{
    clock_state_t s = { .alarm_count = 1, .reminder_count = 1, .timer_count = 1, .last_seen = SAT_1210 };
    s.alarms[0] = alarm_at(12, 11, CLOCK_EVERY_DAY);
    s.reminders[0] = (clock_reminder_t){ .id = 10, .when = SAT_1210 + 30 };
    s.timers[0] = (clock_timer_t){ .id = 20, .total_ms = 1000, .left_ms = 1000, .deadline = 1000 };
    clock_event_t events[CLOCK_MAX_EVENTS];
    CHECK(clock_collect(&s, false, SAT_1210 + 90, 2000, events, CLOCK_MAX_EVENTS) == 1);
    CHECK(events[0].kind == CLOCK_EVENT_TIMER);
    CHECK(s.last_seen == SAT_1210 && !s.reminders[0].fired);
    CHECK(clock_collect(&s, true, SAT_1210 + 90, 2000, events, CLOCK_MAX_EVENTS) == 2);
}

static void test_timer_ops(void)
{
    clock_timer_t t = { .total_ms = 60000, .left_ms = 60000, .deadline = 61000 };
    CHECK(clock_timer_left(&t, 31000) == 30000);
    clock_timer_pause(&t, 31000);
    CHECK(t.deadline == 0 && t.left_ms == 30000);
    CHECK(clock_timer_left(&t, 99000) == 30000);
    clock_timer_add(&t, 60000, 99000);
    CHECK(t.deadline == 0 && t.left_ms == 90000 && t.total_ms == 120000);
    clock_timer_resume(&t, 100000);
    CHECK(t.deadline == 190000);
    clock_timer_add(&t, 60000, 100000);
    CHECK(t.deadline == 250000);

    clock_timer_t done = { .total_ms = 5000, .left_ms = 0 };
    clock_timer_add(&done, 60000, 7000);
    CHECK(done.deadline == 67000 && done.total_ms == 65000);
}

static void test_stopwatch(void)
{
    clock_stopwatch_t sw = { 0 };
    clock_stopwatch_toggle(&sw, 1000);
    clock_stopwatch_lap(&sw, 3500);
    clock_stopwatch_lap(&sw, 4000);
    CHECK(sw.lap_count == 2 && sw.laps[0] == 2500 && sw.laps[1] == 3000);
    clock_stopwatch_toggle(&sw, 6000);
    CHECK(clock_stopwatch_elapsed(&sw, 9000) == 5000);
    clock_stopwatch_lap(&sw, 9000);
    CHECK(sw.lap_count == 2);
    clock_stopwatch_toggle(&sw, 10000);
    CHECK(clock_stopwatch_elapsed(&sw, 11000) == 6000);
    clock_stopwatch_reset(&sw);
    CHECK(clock_stopwatch_elapsed(&sw, 12000) == 0 && sw.lap_count == 0);
}

static void test_formatting(void)
{
    char out[CLOCK_FORMAT_MAX], frac[8];
    clock_format_duration(431000, out, sizeof(out));
    CHECK(strcmp(out, "7:11") == 0);
    clock_format_duration(430001, out, sizeof(out));
    CHECK(strcmp(out, "7:11") == 0);
    clock_format_duration(3900000, out, sizeof(out));
    CHECK(strcmp(out, "1:05:00") == 0);
    clock_format_duration(0, out, sizeof(out));
    CHECK(strcmp(out, "0:00") == 0);

    clock_format_stopwatch(12430, out, sizeof(out), frac, sizeof(frac));
    CHECK(strcmp(out, "00:12") == 0 && strcmp(frac, ".43") == 0);
    clock_format_stopwatch(3723450, out, sizeof(out), frac, sizeof(frac));
    CHECK(strcmp(out, "1:02:03") == 0 && strcmp(frac, ".45") == 0);

    clock_format_days(0, out, sizeof(out));
    CHECK(strcmp(out, "Once") == 0);
    clock_format_days(CLOCK_WEEKDAYS, out, sizeof(out));
    CHECK(strcmp(out, "Mon–Fri") == 0);
    clock_format_days(CLOCK_WEEKEND, out, sizeof(out));
    CHECK(strcmp(out, "Sat, Sun") == 0);
    clock_format_days(CLOCK_EVERY_DAY, out, sizeof(out));
    CHECK(strcmp(out, "Every day") == 0);
    clock_format_days(1 | 4 | 16, out, sizeof(out));
    CHECK(strcmp(out, "Mon, Wed, Fri") == 0);

    clock_format_until(5 * 60, out, sizeof(out));
    CHECK(strcmp(out, "5 min") == 0);
    clock_format_until(18 * HOUR + 20 * 60, out, sizeof(out));
    CHECK(strcmp(out, "18 h 20 min") == 0);
    clock_format_until(DAY + 3 * HOUR, out, sizeof(out));
    CHECK(strcmp(out, "1 d 3 h") == 0);
    clock_format_until(10, out, sizeof(out));
    CHECK(strcmp(out, "1 min") == 0);
}

static void test_round_trip(void)
{
    static char text[CLOCK_TEXT_MAX];
    clock_state_t s = { .alarm_count = 1, .reminder_count = 1, .timer_count = 2, .last_seen = SAT_1210, .next_id = 9 };
    s.alarms[0] = alarm_at(7, 5, CLOCK_WEEKDAYS);
    s.alarms[0].snooze = 15;
    s.alarms[0].snoozed_until = SAT_1210 + 900;
    strcpy(s.alarms[0].label, "Wake\tup");
    s.reminders[0] = (clock_reminder_t){ .id = 3, .when = SAT_1210 + HOUR, .fired = true };
    strcpy(s.reminders[0].text, "Call the electrician");
    s.timers[0] = (clock_timer_t){ .id = 4, .total_ms = 600000, .left_ms = 600000, .deadline = 100000 + 431000 };
    strcpy(s.timers[0].label, "Pasta");
    s.timers[1] = (clock_timer_t){ .id = 5, .total_ms = 2700000, .left_ms = 1984000 };

    int64_t now_ms = (int64_t)SAT_1210 * 1000;
    clock_state_format(&s, now_ms, 100000, text, sizeof(text));
    clock_state_t back;
    clock_state_parse(text, now_ms + 60000, 500, &back);
    CHECK(back.next_id == 9 && back.last_seen == SAT_1210);
    CHECK(back.alarm_count == 1 && back.alarms[0].hour == 7 && back.alarms[0].minute == 5);
    CHECK(back.alarms[0].days == CLOCK_WEEKDAYS && back.alarms[0].on && back.alarms[0].snooze == 15);
    CHECK(back.alarms[0].snoozed_until == SAT_1210 + 900);
    CHECK(strcmp(back.alarms[0].label, "Wake up") == 0);
    CHECK(back.reminder_count == 1 && back.reminders[0].fired && !back.reminders[0].done);
    CHECK(strcmp(back.reminders[0].text, "Call the electrician") == 0);
    CHECK(back.timer_count == 2);
    CHECK(clock_timer_left(&back.timers[0], 500) == 431000 - 60000);
    CHECK(back.timers[1].deadline == 0 && back.timers[1].left_ms == 1984000);

    clock_state_parse("alarm\t1\t25\t0\t0\t1\t10\t0\tBad\nalarm\t2\t6\t30\t0\t1\t10\t0\t\nnonsense\n", 0, 0, &back);
    CHECK(back.alarm_count == 1 && back.alarms[0].id == 2 && back.alarms[0].label[0] == '\0');
}

int main(void)
{
    test_next_alarm();
    test_next_alarm_dst();
    test_collect_alarms();
    test_collect_first_run();
    test_collect_reminders_and_timers();
    test_collect_unsynced();
    test_timer_ops();
    test_stopwatch();
    test_formatting();
    test_round_trip();
    return failures ? 1 : 0;
}
