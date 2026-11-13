#ifndef SLATE_APPS_H
#define SLATE_APPS_H

#include <stdbool.h>
#include <stddef.h>

#define APPS_MAX 16
#define APPS_ID_MAX 32
#define APPS_NAME_MAX 48
#define APPS_VERSION_MAX 32
#define APPS_PERMISSIONS_MAX 256
#define APPS_PATH_MAX 160
#define APPS_TITLE_MAX 48
#define APPS_DETAIL_MAX 96
#define APPS_DIR "/usr/share/slate/apps"
#define APPS_GRANTED_DIR "/run/slate/apps"
#define APPS_PERMISSIONS_DIR "/usr/share/slate/permissions"
#define APPS_IMAGE_FILE "/etc/slate-release"

typedef struct {
    char id[APPS_ID_MAX];
    char name[APPS_NAME_MAX];
    char version[APPS_VERSION_MAX];
    char requested[APPS_PERMISSIONS_MAX];
    char granted[APPS_PERMISSIONS_MAX];
    char logo[APPS_PATH_MAX];
    bool registration;
    bool dev;
} app_t;

bool apps_valid_id(const char *id);
void apps_parse_app(const char *text, const char *id, app_t *out);
bool apps_has(const char *list, const char *word);
size_t apps_count(const char *list);
void apps_sort(app_t *apps, size_t count);

size_t apps_list(app_t *out, size_t max);
bool apps_permission_text(const char *permission, char *title, size_t title_len, char *detail, size_t detail_len);
bool apps_start(const char *id);
bool apps_restart(const char *id);
bool apps_running(const char *id);

#endif
