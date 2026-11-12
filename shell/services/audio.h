#ifndef SLATE_AUDIO_H
#define SLATE_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AUDIO_NAME_MAX 64
#define AUDIO_KIND_MAX 48
#define AUDIO_MAX_DEVICES 8
#define AUDIO_LEVEL_MEDIA "sound_media"
#define AUDIO_LEVEL_ALARM "sound_alarm"
#define AUDIO_LEVEL_NOTIFICATION "sound_notification"
#define AUDIO_LEVEL_DUCK "sound_duck"

typedef enum {
    AUDIO_OUTPUT,
    AUDIO_INPUT,
} audio_direction_t;

typedef enum {
    AUDIO_ROLE_MEDIA,
    AUDIO_ROLE_ALARM,
    AUDIO_ROLE_NOTIFICATION,
    AUDIO_ROLE_COUNT,
} audio_role_t;

#define AUDIO_ADDRESS_MAX 18

typedef struct {
    uint32_t id;
    char name[AUDIO_NAME_MAX];
    char kind[AUDIO_KIND_MAX];
    char address[AUDIO_ADDRESS_MAX];
    bool is_default;
    bool music;
    bool loopback;
    bool internal;
} audio_device_t;

bool audio_parse_volume(const char *text, int *percent, bool *muted);
size_t audio_parse_status(const char *text, audio_direction_t direction, audio_device_t *out, size_t max);
const char *audio_kind_label(const char *inspect, audio_direction_t direction);
void audio_parse_inspect(const char *inspect, audio_device_t *out);
void audio_device_detail(const audio_device_t *device, audio_direction_t direction, const char *bt_kind, char *out,
                         size_t len);

bool audio_get_volume(audio_direction_t direction, int *percent, bool *muted);
bool audio_set_volume(audio_direction_t direction, int percent);
bool audio_set_muted(audio_direction_t direction, bool muted);
size_t audio_list(audio_direction_t direction, audio_device_t *out, size_t max);
bool audio_set_default(uint32_t id);

const char *audio_role_media(audio_role_t role);
bool audio_parse_role_ids(const char *status, uint32_t ids[AUDIO_ROLE_COUNT]);
bool audio_set_role_volume(audio_role_t role, int percent);
bool audio_set_duck_level(int percent);
bool audio_apply_levels(void);
int audio_level(const char *key);
void audio_set_level(const char *key, int percent);

#endif
