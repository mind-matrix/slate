#include "audio.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prefs.h"

#define VOLUME_TAG "[vol:"

bool audio_parse_volume(const char *text, int *percent, bool *muted)
{
    double volume;
    if (sscanf(text, "Volume: %lf", &volume) != 1 || volume < 0)
        return false;
    *percent = (int)(volume * 100 + 0.5);
    *muted = strstr(text, "[MUTED]") != NULL;
    return true;
}

static bool line_has(const char *line, size_t len, const char *needle)
{
    size_t n = strlen(needle);
    for (size_t i = 0; i + n <= len; i++)
        if (memcmp(line + i, needle, n) == 0)
            return true;
    return false;
}

static bool parse_entry(const char *line, size_t len, audio_device_t *out)
{
    const char *end = line + len;
    const char *p = line;
    bool is_default = false;

    while (p < end && !isdigit((unsigned char)*p)) {
        if (*p == '*')
            is_default = true;
        p++;
    }
    if (p == end)
        return false;

    char *after = NULL;
    unsigned long id = strtoul(p, &after, 10);
    if (after >= end || *after != '.')
        return false;
    p = after + 1;
    while (p < end && *p == ' ')
        p++;

    const char *name_end = end;
    for (const char *q = p; q + strlen(VOLUME_TAG) <= end; q++)
        if (strncmp(q, VOLUME_TAG, strlen(VOLUME_TAG)) == 0) {
            name_end = q;
            break;
        }
    while (name_end > p && (name_end[-1] == ' ' || name_end[-1] == '\r'))
        name_end--;
    if (name_end == p)
        return false;

    size_t n = (size_t)(name_end - p);
    if (n >= sizeof(out->name))
        n = sizeof(out->name) - 1;
    memset(out, 0, sizeof(*out));
    memcpy(out->name, p, n);
    out->id = (uint32_t)id;
    out->is_default = is_default;
    return true;
}

size_t audio_parse_status(const char *text, audio_direction_t direction, audio_device_t *out, size_t max)
{
    const char *wanted = direction == AUDIO_OUTPUT ? "Sinks:" : "Sources:";
    bool in_audio = false, in_section = false;
    size_t count = 0;

    for (const char *line = text; *line && count < max;) {
        const char *next = strchr(line, '\n');
        size_t len = next ? (size_t)(next - line) : strlen(line);

        if (len > 0 && isalpha((unsigned char)line[0])) {
            in_audio = strncmp(line, "Audio", 5) == 0;
            in_section = false;
        } else if (in_audio && memchr(line, ':', len) && (line_has(line, len, "├─") || line_has(line, len, "└─"))) {
            const char *colon = memchr(line, ':', len);
            size_t wanted_len = strlen(wanted);
            in_section = colon - line + 1 >= (long)wanted_len && strncmp(colon + 1 - wanted_len, wanted, wanted_len) == 0;
        } else if (in_section && parse_entry(line, len, &out[count]))
            count++;

        line = next ? next + 1 : line + len;
    }
    return count;
}

const char *audio_kind_label(const char *inspect, audio_direction_t direction)
{
    if (strstr(inspect, "\"bluez5\"") || strstr(inspect, "bluez_"))
        return direction == AUDIO_OUTPUT ? "Bluetooth speaker" : "Bluetooth · call quality while in use";
    if (strstr(inspect, "hdmi"))
        return "Display audio";
    if (strstr(inspect, ".usb-") || strstr(inspect, "\"usb\""))
        return direction == AUDIO_OUTPUT ? "USB audio" : "USB microphone";
    return direction == AUDIO_OUTPUT ? "Built-in audio" : "Built-in microphone";
}

static const char *const role_nodes[AUDIO_ROLE_COUNT] = {
    [AUDIO_ROLE_MEDIA] = "role.multimedia",
    [AUDIO_ROLE_ALARM] = "role.alarm",
    [AUDIO_ROLE_NOTIFICATION] = "role.notification",
};

static const char *const role_media[AUDIO_ROLE_COUNT] = {
    [AUDIO_ROLE_MEDIA] = "Multimedia",
    [AUDIO_ROLE_ALARM] = "Alarm",
    [AUDIO_ROLE_NOTIFICATION] = "Notification",
};

static const struct {
    const char *key;
    int fallback;
} levels[] = {
    [AUDIO_ROLE_MEDIA] = { AUDIO_LEVEL_MEDIA, 100 },
    [AUDIO_ROLE_ALARM] = { AUDIO_LEVEL_ALARM, 100 },
    [AUDIO_ROLE_NOTIFICATION] = { AUDIO_LEVEL_NOTIFICATION, 70 },
    [AUDIO_ROLE_COUNT] = { AUDIO_LEVEL_DUCK, 30 },
};

const char *audio_role_media(audio_role_t role)
{
    return role < AUDIO_ROLE_COUNT ? role_media[role] : "";
}

bool audio_parse_role_ids(const char *status, uint32_t ids[AUDIO_ROLE_COUNT])
{
    size_t found = 0;
    for (size_t r = 0; r < AUDIO_ROLE_COUNT; r++)
        ids[r] = 0;
    for (const char *line = status; *line;) {
        const char *next = strchr(line, '\n');
        size_t len = next ? (size_t)(next - line) : strlen(line);
        for (size_t r = 0; r < AUDIO_ROLE_COUNT; r++) {
            char node[64];
            snprintf(node, sizeof(node), "input.loopback.sink.%s ", role_nodes[r]);
            if (ids[r] || !line_has(line, len, node) || !line_has(line, len, "[Audio/Sink]"))
                continue;
            const char *p = line;
            while (p < line + len && !isdigit((unsigned char)*p))
                p++;
            ids[r] = (uint32_t)strtoul(p, NULL, 10);
            found += ids[r] != 0;
        }
        line = next ? next + 1 : line + len;
    }
    return found == AUDIO_ROLE_COUNT;
}

static int level_index(const char *key)
{
    for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); i++)
        if (strcmp(levels[i].key, key) == 0)
            return (int)i;
    return -1;
}

int audio_level(const char *key)
{
    int i = level_index(key);
    return i < 0 ? 0 : prefs_get_int(key, levels[i].fallback);
}

static bool apply(int index, int percent)
{
    return index == AUDIO_ROLE_COUNT ? audio_set_duck_level(percent)
                                     : audio_set_role_volume((audio_role_t)index, percent);
}

void audio_set_level(const char *key, int percent)
{
    int i = level_index(key);
    if (i < 0)
        return;
    prefs_set_int(key, percent);
    apply(i, percent);
}

bool audio_apply_levels(void)
{
    bool ok = true;
    for (size_t i = 0; i < sizeof(levels) / sizeof(levels[0]); i++)
        ok = apply((int)i, audio_level(levels[i].key)) && ok;
    return ok;
}
