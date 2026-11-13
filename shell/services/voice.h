#ifndef SLATE_VOICE_H
#define SLATE_VOICE_H

#include <stdbool.h>
#include <stddef.h>

#include "apps.h"
#include "assistant.h"

/* Voice assistants: installed apps granted "assistant", their registration state as reported
 * to slate-system, and which one is selected. */

#define VOICE_MAX APPS_MAX
#define VOICE_TEXT_MAX 160
#define VOICE_KEY_MIC "slate.assistant.mic"

typedef struct {
    app_t app;
    bool running;
    bool reported;
    assistant_state_t state;
    char detail[ASSISTANT_DETAIL_MAX];
    bool capturing;
} voice_t;

typedef enum { VOICE_TONE_MUTED, VOICE_TONE_SUCCESS, VOICE_TONE_WARNING, VOICE_TONE_DANGER } voice_tone_t;
typedef enum { VOICE_ACTION_NONE, VOICE_ACTION_SETUP, VOICE_ACTION_LOGOUT } voice_action_t;

typedef struct {
    char text[VOICE_TEXT_MAX];
    voice_tone_t tone;
    bool busy;
    bool available;
    bool selectable;
    bool needs_setup;
    voice_action_t action;
} voice_row_t;

typedef void (*voice_changed_cb_t)(void *user);
typedef void (*voice_requested_cb_t)(const char *id, const char *reason, void *user);

/* Apps with registration=none count as registered while they run. */
assistant_state_t voice_state(const voice_t *voice);
void voice_describe(const voice_t *voice, bool selected, voice_row_t *out);
/* Parses one line of `pw-metadata` output for a key on subject 0. */
bool voice_parse_metadata(const char *line, const char *key, char *out, size_t len);

size_t voice_list(voice_t *out, size_t max);
bool voice_get_active(char *out, size_t len);
bool voice_set_active(const char *id);
bool voice_request(const char *id, voice_action_t action, const char *token);
void voice_watch(voice_changed_cb_t changed, voice_requested_cb_t requested, void *user);
/* Publishes the assistants' users and the selection to the audio server's gate. */
void voice_sync_audio(void);
/* A descriptor that becomes readable with pw-metadata lines, or -1. */
int voice_mic_monitor(void);

#endif
