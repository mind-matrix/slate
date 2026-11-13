#include "update.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sysinfo.h"

typedef struct {
    const char *name;
    update_state_t state;
} state_name_t;

static const state_name_t state_names[] = {
    { "disabled", UPDATE_DISABLED },       { "current", UPDATE_CURRENT },
    { "checking", UPDATE_CHECKING },       { "available", UPDATE_AVAILABLE },
    { "downloading", UPDATE_DOWNLOADING }, { "installing", UPDATE_INSTALLING },
    { "restart", UPDATE_RESTART },         { "error", UPDATE_ERROR },
};

static long long number(const char *text, const char *key)
{
    char value[32];
    return sysinfo_parse_key_value(text, key, value, sizeof(value)) ? strtoll(value, NULL, 10) : 0;
}

void update_parse_status(const char *text, update_status_t *out)
{
    char state[UPDATE_TEXT_MAX];
    *out = (update_status_t){ .state = UPDATE_UNKNOWN };
    if (sysinfo_parse_key_value(text, "state", state, sizeof(state)))
        for (size_t i = 0; i < sizeof(state_names) / sizeof(state_names[0]); i++)
            if (strcmp(state, state_names[i].name) == 0)
                out->state = state_names[i].state;
    sysinfo_parse_key_value(text, "version", out->version, sizeof(out->version));
    sysinfo_parse_key_value(text, "reason", out->reason, sizeof(out->reason));
    out->size = (uint64_t)number(text, "size");
    out->progress = (int)number(text, "progress");
    out->checked = number(text, "checked");
    if (out->progress < 0)
        out->progress = 0;
    if (out->progress > 100)
        out->progress = 100;
}

size_t update_parse_notes(const char *text, update_note_t *notes, size_t max)
{
    size_t count = 0;
    for (const char *line = text; line && *line && count < max;) {
        const char *next = strchr(line, '\n');
        size_t len = next ? (size_t)(next - line) : strlen(line);
        if (len > 0) {
            if (len >= UPDATE_NOTE_MAX)
                len = UPDATE_NOTE_MAX - 1;
            memcpy(notes[count], line, len);
            notes[count][len] = '\0';
            count++;
        }
        line = next ? next + 1 : NULL;
    }
    return count;
}

bool update_is_busy(update_state_t state)
{
    return state == UPDATE_CHECKING || state == UPDATE_DOWNLOADING || state == UPDATE_INSTALLING;
}

bool update_check_due(const update_status_t *status, int64_t now)
{
    if (status->state == UPDATE_DISABLED || status->state == UPDATE_RESTART || update_is_busy(status->state))
        return false;
    return now - status->checked >= UPDATE_DAY_SECONDS;
}

void update_format_size(uint64_t bytes, char *out, size_t len)
{
    if (bytes >= 1024 * 1024)
        snprintf(out, len, "%llu MB", (unsigned long long)((bytes + 512 * 1024) / (1024 * 1024)));
    else
        snprintf(out, len, "%llu KB", (unsigned long long)((bytes + 512) / 1024));
}

void update_app_line(const update_status_t *status, const char *installed, bool dev, update_app_line_t *out)
{
    char size[24];
    const char *version = installed && installed[0] ? installed : "unknown";
    out->tone = UPDATE_TONE_MUTED;
    out->action = NULL;
    if (dev) {
        snprintf(out->text, sizeof(out->text), "Version %s · dev build, not updated", version);
        return;
    }
    switch (status->state) {
    case UPDATE_AVAILABLE:
        update_format_size(status->size, size, sizeof(size));
        snprintf(out->text, sizeof(out->text), "Version %s available · %s", status->version, size);
        out->tone = UPDATE_TONE_ACCENT;
        out->action = "Update";
        break;
    case UPDATE_DOWNLOADING:
        snprintf(out->text, sizeof(out->text), "Downloading %s · %d%%", status->version, status->progress);
        out->tone = UPDATE_TONE_ACCENT;
        break;
    case UPDATE_INSTALLING:
        snprintf(out->text, sizeof(out->text), "Installing %s…", status->version);
        out->tone = UPDATE_TONE_ACCENT;
        break;
    case UPDATE_CHECKING:
        snprintf(out->text, sizeof(out->text), "Version %s · checking…", version);
        break;
    case UPDATE_ERROR:
        snprintf(out->text, sizeof(out->text), "Update didn't finish · version %s kept", version);
        out->tone = UPDATE_TONE_DANGER;
        out->action = "Try again";
        break;
    case UPDATE_CURRENT:
        snprintf(out->text, sizeof(out->text), "Version %s · up to date", version);
        break;
    default:
        snprintf(out->text, sizeof(out->text), "Version %s", version);
        break;
    }
}
