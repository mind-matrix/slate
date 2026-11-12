#include <stdio.h>
#include <string.h>
#include "audio.h"

typedef struct {
    int percent;
    bool muted;
} fake_level_t;

static fake_level_t levels[] = { { 44, false }, { 100, false } };
static uint32_t default_output = 47;

static const audio_device_t outputs[] = {
    { .id = 47, .name = "vivo Buds", .kind = "Bluetooth speaker", .address = "20:A3:66:78:0D:2E", .music = true },
    { .id = 48, .name = "Pico", .kind = "Bluetooth speaker", .address = "41:42:2D:9F:C7:0F", .music = true },
    { .id = 49, .name = "HDMI", .kind = "Display audio" },
};
static const audio_device_t inputs[] = {
    { .id = 50, .name = "vivo Buds", .kind = "Bluetooth · call quality while in use", .address = "20:A3:66:78:0D:2E",
      .loopback = true, .is_default = true },
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

bool audio_set_role_volume(audio_role_t role, int percent)
{
    (void)percent;
    return role < AUDIO_ROLE_COUNT;
}

bool audio_set_duck_level(int percent)
{
    (void)percent;
    return true;
}
