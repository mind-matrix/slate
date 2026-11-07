#ifndef SLATE_CHIME_H
#define SLATE_CHIME_H

#include <stdbool.h>

#define CHIME_SOUND_DIR "/usr/share/slate/sounds"
#define CHIME_RAMP_STEPS 4

typedef enum {
    CHIME_ALARM,
    CHIME_TIMER,
    CHIME_REMINDER,
} chime_t;

double chime_ramp(unsigned repeat);
const char *chime_file(chime_t sound);
bool chime_repeats(chime_t sound);

void chime_start(chime_t sound);
void chime_stop(void);
void chime_poll(void);

#endif
