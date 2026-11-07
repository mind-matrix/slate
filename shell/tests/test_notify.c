#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "check.h"
#include "notify.h"

static notify_t make(notify_owner_t owner, const char *key, const char *title)
{
    notify_t n = { .owner = owner, .icon = NOTIFY_ICON_ALARM, .time = 1762500000 };
    snprintf(n.key, sizeof(n.key), "%s", key);
    snprintf(n.title, sizeof(n.title), "%s", title);
    return n;
}

static void test_post_and_replace(void)
{
    notify_list_t list = { 0 };
    notify_t a = make(NOTIFY_CLOCK, "timer:1", "Timer done");
    notify_t b = make(NOTIFY_UPDATE, "available", "Update available");
    notify_list_post(&list, &a);
    notify_list_post(&list, &b);
    CHECK(list.count == 2);
    CHECK(strcmp(list.items[0].key, "available") == 0);

    notify_t again = make(NOTIFY_CLOCK, "timer:1", "Timer done again");
    notify_list_post(&list, &again);
    CHECK(list.count == 2);
    CHECK(strcmp(list.items[0].title, "Timer done again") == 0);
    CHECK(strcmp(list.items[1].key, "available") == 0);

    notify_t other_owner = make(NOTIFY_BLUETOOTH, "timer:1", "Same key, other owner");
    notify_list_post(&list, &other_owner);
    CHECK(list.count == 3);
}

static void test_remove(void)
{
    notify_list_t list = { 0 };
    notify_t a = make(NOTIFY_CLOCK, "a", "A");
    notify_t b = make(NOTIFY_CLOCK, "b", "B");
    notify_list_post(&list, &a);
    notify_list_post(&list, &b);
    CHECK(notify_list_remove(&list, NOTIFY_CLOCK, "a"));
    CHECK(list.count == 1 && strcmp(list.items[0].key, "b") == 0);
    CHECK(!notify_list_remove(&list, NOTIFY_CLOCK, "a"));
    CHECK(!notify_list_remove(&list, NOTIFY_UPDATE, "b"));
}

static void test_cap(void)
{
    notify_list_t list = { 0 };
    char key[16];
    for (int i = 0; i < NOTIFY_MAX + 5; i++) {
        snprintf(key, sizeof(key), "k%d", i);
        notify_t n = make(NOTIFY_CLOCK, key, "x");
        notify_list_post(&list, &n);
    }
    CHECK(list.count == NOTIFY_MAX);
    snprintf(key, sizeof(key), "k%d", NOTIFY_MAX + 4);
    CHECK(strcmp(list.items[0].key, key) == 0);
    CHECK(strcmp(list.items[NOTIFY_MAX - 1].key, "k5") == 0);
}

static void test_unseen(void)
{
    notify_list_t list = { 0 };
    notify_t a = make(NOTIFY_CLOCK, "a", "A");
    notify_t b = make(NOTIFY_CLOCK, "b", "B");
    b.seen = true;
    notify_list_post(&list, &a);
    notify_list_post(&list, &b);
    CHECK(notify_list_unseen(&list) == 1);
}

static void test_round_trip(void)
{
    static char text[NOTIFY_TEXT_MAX];
    notify_list_t list = { 0 }, back;
    notify_t a = make(NOTIFY_CLOCK, "reminder:3", "Water\tthe plants");
    snprintf(a.body, sizeof(a.body), "Line one\nline two");
    a.icon = NOTIFY_ICON_REMINDER;
    notify_t b = make(NOTIFY_BLUETOOTH, "pair-failed:/org/bluez/hci0/dev_AA", "Pairing failed");
    b.seen = true;
    notify_list_post(&list, &a);
    notify_list_post(&list, &b);

    notify_list_format(&list, text, sizeof(text));
    notify_list_parse(text, &back);
    CHECK(back.count == 2);
    CHECK(back.items[0].owner == NOTIFY_BLUETOOTH && back.items[0].seen);
    CHECK(strcmp(back.items[0].key, "pair-failed:/org/bluez/hci0/dev_AA") == 0);
    CHECK(back.items[1].icon == NOTIFY_ICON_REMINDER && !back.items[1].seen);
    CHECK(strcmp(back.items[1].title, "Water the plants") == 0);
    CHECK(strcmp(back.items[1].body, "Line one line two") == 0);
    CHECK(back.items[1].time == 1762500000);

    notify_list_parse("9\t0\t0\t1\tkey\tt\tb\n0\t0\t0\t1\t\tt\tb\nbroken line\n0\t1\t0\t5\tok\tTitle\t\n", &back);
    CHECK(back.count == 1);
    CHECK(strcmp(back.items[0].key, "ok") == 0 && back.items[0].body[0] == '\0');
}

static void test_format_when(void)
{
    setenv("TZ", "UTC", 1);
    tzset();
    char out[NOTIFY_WHEN_MAX];
    int64_t now = 1794053400;
    notify_format_when(now - 3600, now, out, sizeof(out));
    CHECK(strcmp(out, "11:10") == 0);
    notify_format_when(now - 86400, now, out, sizeof(out));
    CHECK(strcmp(out, "Yesterday") == 0);
    notify_format_when(now - 3 * 86400, now, out, sizeof(out));
    CHECK(strcmp(out, "Wed 4 Nov") == 0);
    CHECK(strcmp(notify_owner_label(NOTIFY_UPDATE), "Software update") == 0);
}

int main(void)
{
    test_post_and_replace();
    test_remove();
    test_cap();
    test_unseen();
    test_round_trip();
    test_format_when();
    return failures ? 1 : 0;
}
