#include "voice.h"

#include <stdio.h>
#include <string.h>

assistant_state_t voice_state(const voice_t *voice)
{
    if (!voice->app.registration)
        return ASSISTANT_REGISTERED;
    return voice->reported ? voice->state : ASSISTANT_INITIALIZING;
}

void voice_describe(const voice_t *voice, bool selected, voice_row_t *out)
{
    const char *name = voice->app.name;
    memset(out, 0, sizeof(*out));
    out->available = voice->running;
    if (!voice->running) {
        snprintf(out->text, sizeof(out->text), "Not running");
        return;
    }
    bool required = voice->app.registration;
    switch (voice_state(voice)) {
    case ASSISTANT_INITIALIZING:
        snprintf(out->text, sizeof(out->text), "Starting…");
        out->busy = true;
        break;
    case ASSISTANT_UNREGISTERED:
        snprintf(out->text, sizeof(out->text), "%s needs to be set up before use", name);
        out->tone = VOICE_TONE_WARNING;
        out->action = VOICE_ACTION_SETUP;
        out->needs_setup = true;
        break;
    case ASSISTANT_REGISTERING:
        snprintf(out->text, sizeof(out->text), "Setting up in %s…", name);
        out->busy = true;
        break;
    case ASSISTANT_REGISTERED:
        snprintf(out->text, sizeof(out->text), selected ? "Active · Ready" : "Ready");
        out->tone = selected ? VOICE_TONE_SUCCESS : VOICE_TONE_MUTED;
        out->selectable = true;
        out->action = required ? VOICE_ACTION_LOGOUT : VOICE_ACTION_NONE;
        break;
    case ASSISTANT_UNREGISTERING:
        snprintf(out->text, sizeof(out->text), "Logging out…");
        out->busy = true;
        break;
    default:
        if (voice->detail[0])
            snprintf(out->text, sizeof(out->text), "%s", voice->detail);
        else
            snprintf(out->text, sizeof(out->text), "Something went wrong in %s", name);
        out->tone = VOICE_TONE_DANGER;
        out->action = VOICE_ACTION_SETUP;
        out->needs_setup = true;
        break;
    }
}

bool voice_parse_metadata(const char *line, const char *key, char *out, size_t len)
{
    char needle[96];
    snprintf(needle, sizeof(needle), "id:0 key:'%s' value:'", key);
    const char *p = strstr(line, needle);
    if (!p)
        return false;
    p += strlen(needle);
    const char *end = strchr(p, '\'');
    if (!end)
        return false;
    snprintf(out, len, "%.*s", (int)(end - p), p);
    return true;
}
