#include "assistant.h"

#include <string.h>

static const char *const names[ASSISTANT_STATE_COUNT] = {
    [ASSISTANT_INITIALIZING] = "initializing", [ASSISTANT_UNREGISTERED] = "unregistered",
    [ASSISTANT_REGISTERING] = "registering",   [ASSISTANT_REGISTERED] = "registered",
    [ASSISTANT_UNREGISTERING] = "unregistering", [ASSISTANT_ERROR] = "error",
};

bool assistant_parse_state(const char *name, assistant_state_t *out)
{
    for (int i = 0; name && i < ASSISTANT_STATE_COUNT; i++)
        if (strcmp(name, names[i]) == 0) {
            *out = (assistant_state_t)i;
            return true;
        }
    return false;
}

const char *assistant_state_name(assistant_state_t state)
{
    return state < ASSISTANT_STATE_COUNT ? names[state] : names[ASSISTANT_INITIALIZING];
}

bool assistant_valid_activity(const char *activity)
{
    return activity && (strcmp(activity, ASSISTANT_ACTIVITY_CAPTURING) == 0 || strcmp(activity, ASSISTANT_ACTIVITY_IDLE) == 0);
}

bool assistant_valid_text(const char *text, size_t max)
{
    if (!text)
        return false;
    size_t n = strlen(text);
    if (n >= max)
        return false;
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)text[i] < 0x20 || text[i] == 0x7F)
            return false;
    return true;
}
