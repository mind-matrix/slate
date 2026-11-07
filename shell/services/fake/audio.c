#include <stdio.h>
#include <string.h>
#include "audio.h"

typedef struct {
    int percent;
    bool muted;
} fake_level_t;

static fake_level_t levels[] = { { 44, false }, { 100, false } };
static uint32_t default_output = 48;

static const audio_device_t outputs[] = {
    { 48, "Pico", "Bluetooth speaker", false },
    { 49, "HDMI", "Display audio", false },
};
static const audio_device_t inputs[] = {
    { 50, "Pico", "Bluetooth · call quality while in use", true },
};

bool audio_get_volume(audio_direction_t direction, int *percent, bool *muted)
{
    *percent = levels[direction].percent;
    *muted = levels[direction].muted;
    return true;
}

bool audio_set_volume(audio_direction_t direction, int percent)
{
    levels[direction].percent = percent;
    levels[direction].muted = false;
    return true;
}

bool audio_set_muted(audio_direction_t direction, bool muted)
{
    levels[direction].muted = muted;
    return true;
}

size_t audio_list(audio_direction_t direction, audio_device_t *out, size_t max)
{
    const audio_device_t *source = direction == AUDIO_OUTPUT ? outputs : inputs;
    size_t count = direction == AUDIO_OUTPUT ? sizeof(outputs) / sizeof(outputs[0]) : sizeof(inputs) / sizeof(inputs[0]);
    size_t n = 0;
    for (; n < count && n < max; n++) {
        out[n] = source[n];
        if (direction == AUDIO_OUTPUT)
            out[n].is_default = out[n].id == default_output;
    }
    return n;
}

bool audio_set_default(uint32_t id)
{
    default_output = id;
    return true;
}
