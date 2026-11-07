#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio.h"

#define SINK "@DEFAULT_AUDIO_SINK@"
#define SOURCE "@DEFAULT_AUDIO_SOURCE@"
#define STATUS_BUF (16 * 1024)
#define INSPECT_BUF (8 * 1024)

static const char *target(audio_direction_t direction)
{
    return direction == AUDIO_OUTPUT ? SINK : SOURCE;
}

static size_t run(const char *command, char *out, size_t len)
{
    FILE *pipe = popen(command, "r");
    if (!pipe)
        return 0;
    size_t n = fread(out, 1, len - 1, pipe);
    out[n] = '\0';
    return pclose(pipe) == 0 ? n : 0;
}

bool audio_get_volume(audio_direction_t direction, int *percent, bool *muted)
{
    char command[96], text[64];
    snprintf(command, sizeof(command), "wpctl get-volume %s 2>/dev/null", target(direction));
    return run(command, text, sizeof(text)) && audio_parse_volume(text, percent, muted);
}

bool audio_set_volume(audio_direction_t direction, int percent)
{
    char command[160];
    snprintf(command, sizeof(command), "wpctl set-mute %s 0 && wpctl set-volume -l 1.0 %s %d%% 2>/dev/null",
             target(direction), target(direction), percent);
    return system(command) == 0;
}

bool audio_set_muted(audio_direction_t direction, bool muted)
{
    char command[96];
    snprintf(command, sizeof(command), "wpctl set-mute %s %d 2>/dev/null", target(direction), muted ? 1 : 0);
    return system(command) == 0;
}

size_t audio_list(audio_direction_t direction, audio_device_t *out, size_t max)
{
    char *status = malloc(STATUS_BUF);
    char *inspect = malloc(INSPECT_BUF);
    size_t count = 0;

    if (status && inspect && run("wpctl status 2>/dev/null", status, STATUS_BUF))
        count = audio_parse_status(status, direction, out, max);
    for (size_t i = 0; i < count; i++) {
        char command[64];
        snprintf(command, sizeof(command), "wpctl inspect %u 2>/dev/null", out[i].id);
        const char *kind = run(command, inspect, INSPECT_BUF) ? audio_kind_label(inspect, direction) : "";
        snprintf(out[i].kind, sizeof(out[i].kind), "%s", kind);
    }
    free(status);
    free(inspect);
    return count;
}

bool audio_set_default(uint32_t id)
{
    char command[64];
    snprintf(command, sizeof(command), "wpctl set-default %u 2>/dev/null", id);
    return system(command) == 0;
}

bool audio_set_role_volume(audio_role_t role, int percent)
{
    char *status = malloc(STATUS_BUF);
    uint32_t ids[AUDIO_ROLE_COUNT];
    bool found = status && run("wpctl status 2>/dev/null", status, STATUS_BUF) && audio_parse_role_ids(status, ids);
    free(status);
    if (!found || role >= AUDIO_ROLE_COUNT)
        return false;
    char command[96];
    snprintf(command, sizeof(command), "wpctl set-volume %u %d%% >/dev/null 2>&1", ids[role], percent);
    return system(command) == 0;
}

bool audio_set_duck_level(int percent)
{
    char command[112];
    snprintf(command, sizeof(command), "wpctl settings linking.role-based.duck-level %d.%02d >/dev/null 2>&1",
             percent / 100, percent % 100);
    return system(command) == 0;
}
