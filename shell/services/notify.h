#ifndef SLATE_NOTIFY_H
#define SLATE_NOTIFY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NOTIFY_MAX 50
#define NOTIFY_KEY_MAX 96
#define NOTIFY_TITLE_MAX 64
#define NOTIFY_BODY_MAX 128
#define NOTIFY_WHEN_MAX 24
#define NOTIFY_TEXT_MAX (NOTIFY_MAX * 320)

typedef enum {
    NOTIFY_CLOCK,
    NOTIFY_UPDATE,
    NOTIFY_BLUETOOTH,
    NOTIFY_OWNER_COUNT,
} notify_owner_t;

typedef enum {
    NOTIFY_ICON_ALARM,
    NOTIFY_ICON_TIMER,
    NOTIFY_ICON_REMINDER,
    NOTIFY_ICON_UPDATE,
    NOTIFY_ICON_BLUETOOTH,
    NOTIFY_ICON_COUNT,
} notify_icon_t;

typedef struct {
    notify_owner_t owner;
    notify_icon_t icon;
    char key[NOTIFY_KEY_MAX];
    char title[NOTIFY_TITLE_MAX];
    char body[NOTIFY_BODY_MAX];
    int64_t time;
    bool seen;
} notify_t;

typedef struct {
    notify_t items[NOTIFY_MAX];
    size_t count;
} notify_list_t;

typedef void (*notify_dismiss_cb_t)(const char *key);
typedef void (*notify_change_cb_t)(void);

void notify_list_post(notify_list_t *list, const notify_t *item);
bool notify_list_remove(notify_list_t *list, notify_owner_t owner, const char *key);
size_t notify_list_unseen(const notify_list_t *list);
void notify_list_format(const notify_list_t *list, char *out, size_t len);
void notify_list_parse(const char *text, notify_list_t *list);
const char *notify_owner_label(notify_owner_t owner);
void notify_format_when(int64_t time, int64_t now, char *out, size_t len);

void notify_load(void);
void notify_post(notify_owner_t owner, notify_icon_t icon, const char *key, const char *title, const char *body);
void notify_remove(notify_owner_t owner, const char *key);
void notify_dismiss(size_t index);
void notify_clear_all(void);
void notify_mark_seen(void);
const notify_list_t *notify_items(void);
bool notify_has(notify_owner_t owner, const char *key);
void notify_on_dismiss(notify_owner_t owner, notify_dismiss_cb_t cb);
void notify_on_change(notify_change_cb_t cb);

#endif
