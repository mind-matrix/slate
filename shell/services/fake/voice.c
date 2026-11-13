#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "voice.h"

/* Marvin is ready and selected; Ava needs setup, which completes a few reads after it starts. */

static char active[APPS_ID_MAX] = "marvin";
static assistant_state_t ava = ASSISTANT_UNREGISTERED;
static int ava_steps;

size_t voice_list(voice_t *out, size_t max)
{
    app_t apps[APPS_MAX];
    size_t count = apps_list(apps, APPS_MAX), n = 0;
    if (ava_steps > 0 && --ava_steps == 0)
        ava = ava == ASSISTANT_REGISTERING ? ASSISTANT_REGISTERED : ASSISTANT_UNREGISTERED;
    for (size_t i = 0; i < count && n < max; i++) {
        if (!apps_has(apps[i].granted, "assistant"))
            continue;
        memset(&out[n], 0, sizeof(out[n]));
        out[n].app = apps[i];
        out[n].running = true;
        out[n].reported = true;
        out[n].state = strcmp(apps[i].id, "ava") == 0 ? ava : ASSISTANT_REGISTERED;
        out[n].capturing = getenv("SLATE_FAKE_CAPTURING") != NULL;
        n++;
    }
    return n;
}

bool voice_get_active(char *out, size_t len)
{
    snprintf(out, len, "%s", active);
    return true;
}

bool voice_set_active(const char *id)
{
    snprintf(active, sizeof(active), "%s", id);
    return true;
}

bool voice_request(const char *id, voice_action_t action, const char *token)
{
    (void)token;
    if (strcmp(id, "ava") != 0)
        return false;
    ava = action == VOICE_ACTION_LOGOUT ? ASSISTANT_UNREGISTERING : ASSISTANT_REGISTERING;
    ava_steps = 3;
    return true;
}

void voice_watch(voice_changed_cb_t changed, voice_requested_cb_t requested, void *user)
{
    (void)changed, (void)requested, (void)user;
}

void voice_sync_audio(void)
{
}

int voice_mic_monitor(void)
{
    return -1;
}
