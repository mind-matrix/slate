#include "notify.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "sysinfo.h"

#define STATE_DIR ".local/state/slate"
#define STATE_FILE "notifications"
#define PATH_LEN 256
#define MAX_LISTENERS 4
#define FIELDS 7

static notify_list_t list;
static notify_dismiss_cb_t dismiss_handlers[NOTIFY_OWNER_COUNT];
static notify_change_cb_t listeners[MAX_LISTENERS];

static const char *const owner_labels[NOTIFY_OWNER_COUNT] = {
    [NOTIFY_CLOCK] = "Clock",
    [NOTIFY_UPDATE] = "Software update",
    [NOTIFY_BLUETOOTH] = "Bluetooth",
};

static ptrdiff_t find(const notify_list_t *l, notify_owner_t owner, const char *key)
{
    for (size_t i = 0; i < l->count; i++)
        if (l->items[i].owner == owner && strcmp(l->items[i].key, key) == 0)
            return (ptrdiff_t)i;
    return -1;
}

static void remove_at(notify_list_t *l, size_t index)
{
    memmove(&l->items[index], &l->items[index + 1], (l->count - index - 1) * sizeof(notify_t));
    l->count--;
}

void notify_list_post(notify_list_t *l, const notify_t *item)
{
    ptrdiff_t existing = find(l, item->owner, item->key);
    if (existing >= 0)
        remove_at(l, (size_t)existing);
    else if (l->count == NOTIFY_MAX)
        l->count--;
    memmove(&l->items[1], &l->items[0], l->count * sizeof(notify_t));
    l->items[0] = *item;
    l->count++;
}

bool notify_list_remove(notify_list_t *l, notify_owner_t owner, const char *key)
{
    ptrdiff_t index = find(l, owner, key);
    if (index < 0)
        return false;
    remove_at(l, (size_t)index);
    return true;
}

size_t notify_list_unseen(const notify_list_t *l)
{
    size_t unseen = 0;
    for (size_t i = 0; i < l->count; i++)
        unseen += !l->items[i].seen;
    return unseen;
}

static void put_field(char *out, size_t len, size_t *used, const char *text, char end)
{
    for (const char *c = text; *c && *used + 2 < len; c++)
        out[(*used)++] = (*c == '\t' || *c == '\n') ? ' ' : *c;
    if (*used + 1 < len)
        out[(*used)++] = end;
    out[*used] = '\0';
}

void notify_list_format(const notify_list_t *l, char *out, size_t len)
{
    size_t used = 0;
    if (len)
        out[0] = '\0';
    for (size_t i = 0; i < l->count; i++) {
        const notify_t *n = &l->items[i];
        char number[24];
        snprintf(number, sizeof(number), "%d", (int)n->owner);
        put_field(out, len, &used, number, '\t');
        snprintf(number, sizeof(number), "%d", (int)n->icon);
        put_field(out, len, &used, number, '\t');
        put_field(out, len, &used, n->seen ? "1" : "0", '\t');
        snprintf(number, sizeof(number), "%lld", (long long)n->time);
        put_field(out, len, &used, number, '\t');
        put_field(out, len, &used, n->key, '\t');
        put_field(out, len, &used, n->title, '\t');
        put_field(out, len, &used, n->body, '\n');
    }
}

static void copy_field(char *out, size_t len, const char *start, size_t n)
{
    if (n >= len)
        n = len - 1;
    memcpy(out, start, n);
    out[n] = '\0';
}

void notify_list_parse(const char *text, notify_list_t *l)
{
    l->count = 0;
    for (const char *line = text; line && *line && l->count < NOTIFY_MAX;) {
        const char *next = strchr(line, '\n');
        size_t line_len = next ? (size_t)(next - line) : strlen(line);
        const char *field[FIELDS];
        size_t field_len[FIELDS];
        size_t count = 0;
        for (const char *p = line; count < FIELDS;) {
            const char *tab = memchr(p, '\t', line_len - (size_t)(p - line));
            field[count] = p;
            field_len[count] = tab ? (size_t)(tab - p) : line_len - (size_t)(p - line);
            count++;
            if (!tab)
                break;
            p = tab + 1;
        }
        if (count == FIELDS) {
            notify_t n = { 0 };
            int owner = atoi(field[0]), icon = atoi(field[1]);
            n.owner = (notify_owner_t)owner;
            n.icon = (notify_icon_t)icon;
            n.seen = field[2][0] == '1';
            n.time = strtoll(field[3], NULL, 10);
            copy_field(n.key, sizeof(n.key), field[4], field_len[4]);
            copy_field(n.title, sizeof(n.title), field[5], field_len[5]);
            copy_field(n.body, sizeof(n.body), field[6], field_len[6]);
            if (owner >= 0 && owner < NOTIFY_OWNER_COUNT && icon >= 0 && icon < NOTIFY_ICON_COUNT && n.key[0])
                l->items[l->count++] = n;
        }
        line = next ? next + 1 : NULL;
    }
}

const char *notify_owner_label(notify_owner_t owner)
{
    return owner < NOTIFY_OWNER_COUNT ? owner_labels[owner] : "";
}

static long day_number(const struct tm *tm)
{
    return (long)tm->tm_year * 400 + tm->tm_yday;
}

void notify_format_when(int64_t when, int64_t now, char *out, size_t len)
{
    time_t a = (time_t)when, b = (time_t)now;
    struct tm then, today;
    localtime_r(&a, &then);
    localtime_r(&b, &today);
    struct tm yesterday = today;
    yesterday.tm_mday--;
    yesterday.tm_isdst = -1;
    time_t y = mktime(&yesterday);
    localtime_r(&y, &yesterday);

    if (day_number(&then) == day_number(&today))
        strftime(out, len, "%H:%M", &then);
    else if (day_number(&then) == day_number(&yesterday))
        snprintf(out, len, "Yesterday");
    else
        strftime(out, len, "%a %-d %b", &then);
}

static bool state_path(char *path, size_t len, bool create_dir)
{
    const char *home = getenv("HOME");
    if (!home || !*home)
        return false;
    if (create_dir) {
        const char *parts[] = { ".local", ".local/state", STATE_DIR };
        char dir[PATH_LEN];
        for (size_t i = 0; i < sizeof(parts) / sizeof(parts[0]); i++) {
            snprintf(dir, sizeof(dir), "%s/%s", home, parts[i]);
            mkdir(dir, 0755);
        }
    }
    return snprintf(path, len, "%s/%s/%s", home, STATE_DIR, STATE_FILE) < (int)len;
}

static void save(void)
{
    static char text[NOTIFY_TEXT_MAX];
    char path[PATH_LEN], tmp[PATH_LEN + 8];
    if (!state_path(path, sizeof(path), true))
        return;
    notify_list_format(&list, text, sizeof(text));
    snprintf(tmp, sizeof(tmp), "%s.new", path);
    FILE *file = fopen(tmp, "w");
    if (!file)
        return;
    bool written = fputs(text, file) >= 0;
    written = fflush(file) == 0 && written;
    if (fclose(file) == 0 && written)
        rename(tmp, path);
}

static void changed(void)
{
    save();
    for (size_t i = 0; i < MAX_LISTENERS && listeners[i]; i++)
        listeners[i]();
}

void notify_load(void)
{
    static char text[NOTIFY_TEXT_MAX];
    char path[PATH_LEN];
    if (state_path(path, sizeof(path), false) && sysinfo_read_file(path, text, sizeof(text)))
        notify_list_parse(text, &list);
}

static void copy_text(char *out, size_t len, const char *text)
{
    snprintf(out, len, "%s", text ? text : "");
}

void notify_post(notify_owner_t owner, notify_icon_t icon, const char *key, const char *title, const char *body)
{
    notify_t item = { .owner = owner, .icon = icon, .time = (int64_t)time(NULL) };
    copy_text(item.key, sizeof(item.key), key);
    copy_text(item.title, sizeof(item.title), title);
    copy_text(item.body, sizeof(item.body), body);
    notify_list_post(&list, &item);
    changed();
}

void notify_remove(notify_owner_t owner, const char *key)
{
    if (notify_list_remove(&list, owner, key))
        changed();
}

static void dismiss_item(const notify_t *item)
{
    if (item->owner < NOTIFY_OWNER_COUNT && dismiss_handlers[item->owner])
        dismiss_handlers[item->owner](item->key);
}

void notify_dismiss(size_t index)
{
    if (index >= list.count)
        return;
    notify_t item = list.items[index];
    remove_at(&list, index);
    dismiss_item(&item);
    changed();
}

void notify_clear_all(void)
{
    notify_list_t old = list;
    list.count = 0;
    for (size_t i = 0; i < old.count; i++)
        dismiss_item(&old.items[i]);
    changed();
}

void notify_mark_seen(void)
{
    if (notify_list_unseen(&list) == 0)
        return;
    for (size_t i = 0; i < list.count; i++)
        list.items[i].seen = true;
    changed();
}

const notify_list_t *notify_items(void)
{
    return &list;
}

bool notify_has(notify_owner_t owner, const char *key)
{
    return find(&list, owner, key) >= 0;
}

void notify_on_dismiss(notify_owner_t owner, notify_dismiss_cb_t cb)
{
    if (owner < NOTIFY_OWNER_COUNT)
        dismiss_handlers[owner] = cb;
}

void notify_on_change(notify_change_cb_t cb)
{
    for (size_t i = 0; i < MAX_LISTENERS; i++) {
        if (!listeners[i]) {
            listeners[i] = cb;
            return;
        }
    }
}
