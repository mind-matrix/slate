#include "chime.h"

static const double ramp[CHIME_RAMP_STEPS] = { 0.4, 0.6, 0.8, 1.0 };

double chime_ramp(unsigned repeat)
{
    return ramp[repeat < CHIME_RAMP_STEPS ? repeat : CHIME_RAMP_STEPS - 1];
}

const char *chime_file(chime_t sound)
{
    switch (sound) {
    case CHIME_ALARM:
        return CHIME_SOUND_DIR "/alarm.wav";
    case CHIME_TIMER:
        return CHIME_SOUND_DIR "/timer.wav";
    case CHIME_REMINDER:
        return CHIME_SOUND_DIR "/chime.wav";
    }
    return "";
}

bool chime_repeats(chime_t sound)
{
    return sound != CHIME_REMINDER;
}
