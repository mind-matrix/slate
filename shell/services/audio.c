#include "audio.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prefs.h"

#define VOLUME_TAG "[vol:"
#define CLASS_TAG "[Audio/"
#define FILTER_SOURCE "[Audio/Source]"

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

static bool starts_with(const char *p, const char *end, const char *tag)
{
    size_t n = strlen(tag);
    return (size_t)(end - p) >= n && memcmp(p, tag, n) == 0;
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
    for (const char *q = p; q < end; q++)
        if (starts_with(q, end, VOLUME_TAG) || starts_with(q, end, CLASS_TAG)) {
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
    bool in_audio = false, in_section = false, in_filters = false;
    size_t count = 0;

    for (const char *line = text; *line && count < max;) {
        const char *next = strchr(line, '\n');
        size_t len = next ? (size_t)(next - line) : strlen(line);

        if (len > 0 && isalpha((unsigned char)line[0])) {
            in_audio = strncmp(line, "Audio", 5) == 0;
            in_section = in_filters = false;
        } else if (in_audio && memchr(line, ':', len) && (line_has(line, len, "├─") || line_has(line, len, "└─"))) {
            const char *colon = memchr(line, ':', len);
            size_t wanted_len = strlen(wanted);
            in_section = colon - line + 1 >= (long)wanted_len && strncmp(colon + 1 - wanted_len, wanted, wanted_len) == 0;
            in_filters = direction == AUDIO_INPUT && line_has(line, len, "Filters:");
        } else if (in_section && parse_entry(line, len, &out[count]))
            count++;
        else if (in_filters && line_has(line, len, FILTER_SOURCE) && parse_entry(line, len, &out[count]))
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

static bool inspect_value(const char *inspect, const char *key, char *out, size_t len)
{
    char needle[64];
    snprintf(needle, sizeof(needle), " %s = \"", key);
    const char *start = strstr(inspect, needle);
    if (!start || len == 0)
        return false;
    start += strlen(needle);
    const char *end = strchr(start, '"');
    if (!end)
        return false;
    size_t n = (size_t)(end - start);
    if (n >= len)
        n = len - 1;
    memcpy(out, start, n);
    out[n] = '\0';
    return true;
}

void audio_parse_inspect(const char *inspect, audio_device_t *out)
{
    char value[AUDIO_NAME_MAX];
    if (inspect_value(inspect, "node.description", value, sizeof(value)) && value[0])
        snprintf(out->name, sizeof(out->name), "%s", value);
    if (!inspect_value(inspect, "api.bluez5.address", out->address, sizeof(out->address)) &&
        inspect_value(inspect, "node.name", value, sizeof(value)) && strncmp(value, "bluez_input.", 12) == 0)
        snprintf(out->address, sizeof(out->address), "%.*s", AUDIO_ADDRESS_MAX - 1, value + 12);
    out->music = inspect_value(inspect, "api.bluez5.profile", value, sizeof(value)) && strstr(value, "a2dp") != NULL;
    out->loopback = inspect_value(inspect, "bluez5.loopback", value, sizeof(value)) && strcmp(value, "true") == 0;
    out->internal = inspect_value(inspect, "node.name", value, sizeof(value)) &&
                    strncmp(value, "bluez_input_internal.", 21) == 0;
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

void audio_device_detail(const audio_device_t *device, audio_direction_t direction, const char *bt_kind, char *out,
                         size_t len)
{
    if (!device->address[0]) {
        snprintf(out, len, "%s", device->kind);
        return;
    }
    char kind[AUDIO_KIND_MAX];
    size_t i = 0;
    for (; bt_kind && bt_kind[i] && i + 1 < sizeof(kind); i++)
        kind[i] = (char)tolower((unsigned char)bt_kind[i]);
    kind[i] = '\0';
    if (!kind[0] || strcmp(kind, "device") == 0)
        snprintf(kind, sizeof(kind), "%s", direction == AUDIO_OUTPUT ? "speaker" : "headset");
    if (direction == AUDIO_OUTPUT)
        snprintf(out, len, "Bluetooth %s · %s quality", kind, device->music ? "music" : "call");
    else if (device->loopback)
        snprintf(out, len, "Bluetooth %s · switches to call quality while the mic is in use", kind);
    else
        snprintf(out, len, "Bluetooth %s · call quality", kind);
}
