#ifndef SLATE_AUDIO_H
#define SLATE_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AUDIO_NAME_MAX 64
#define AUDIO_KIND_MAX 48
#define AUDIO_MAX_DEVICES 8

typedef enum {
    AUDIO_OUTPUT,
    AUDIO_INPUT,
} audio_direction_t;

typedef struct {
    uint32_t id;
    char name[AUDIO_NAME_MAX];
    char kind[AUDIO_KIND_MAX];
    bool is_default;
} audio_device_t;

bool audio_parse_volume(const char *text, int *percent, bool *muted);
size_t audio_parse_status(const char *text, audio_direction_t direction, audio_device_t *out, size_t max);
const char *audio_kind_label(const char *inspect, audio_direction_t direction);

bool audio_get_volume(audio_direction_t direction, int *percent, bool *muted);
bool audio_set_volume(audio_direction_t direction, int percent);
bool audio_set_muted(audio_direction_t direction, bool muted);
size_t audio_list(audio_direction_t direction, audio_device_t *out, size_t max);
bool audio_set_default(uint32_t id);

#endif
