#ifndef SLATE_VOICE_ASSISTANT_SCREEN_H
#define SLATE_VOICE_ASSISTANT_SCREEN_H

#include <stddef.h>
#include "nav/nav.h"

extern const sui_screen_def_t voice_assistant_screen;

/* Watches the assistants: keeps the audio gate and mic badge current, finishes a selection that
 * waited for setup, and posts "<name> is now your voice assistant". */
void voice_assistant_start(void);
void voice_assistant_summary(char *out, size_t len);

#endif
