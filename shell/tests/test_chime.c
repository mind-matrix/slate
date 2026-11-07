#include <string.h>

#include "check.h"
#include "chime.h"

int main(void)
{
    CHECK(chime_ramp(0) < chime_ramp(1));
    CHECK(chime_ramp(CHIME_RAMP_STEPS - 1) == 1.0);
    CHECK(chime_ramp(100) == 1.0);
    CHECK(strcmp(chime_file(CHIME_REMINDER), CHIME_SOUND_DIR "/chime.wav") == 0);
    CHECK(chime_repeats(CHIME_ALARM) && chime_repeats(CHIME_TIMER) && !chime_repeats(CHIME_REMINDER));
    return failures ? 1 : 0;
}
