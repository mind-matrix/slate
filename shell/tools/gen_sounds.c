#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define RATE 22050
#define MAX_SAMPLES (RATE * 3)
#define PI 3.14159265358979323846

typedef struct {
    double start;
    double length;
    double freq;
    double decay;
} note_t;

static int16_t samples[MAX_SAMPLES];

static void put16(FILE *f, uint16_t v)
{
    fputc(v & 0xFF, f);
    fputc(v >> 8, f);
}

static void put32(FILE *f, uint32_t v)
{
    put16(f, v & 0xFFFF);
    put16(f, v >> 16);
}

static int render(const char *path, const note_t *notes, size_t count, double seconds, double gain)
{
    size_t total = (size_t)(seconds * RATE);
    memset(samples, 0, sizeof(samples));
    for (size_t n = 0; n < count; n++) {
        size_t first = (size_t)(notes[n].start * RATE), length = (size_t)(notes[n].length * RATE);
        for (size_t i = 0; i < length && first + i < total; i++) {
            double t = (double)i / RATE;
            double attack = fmin(1.0, t / 0.005);
            double release = fmin(1.0, (notes[n].length - t) / 0.01);
            double env = attack * release * exp(-notes[n].decay * t);
            double tone = sin(2 * PI * notes[n].freq * t) + 0.35 * sin(4 * PI * notes[n].freq * t) +
                          0.12 * sin(6 * PI * notes[n].freq * t);
            double mixed = samples[first + i] + gain * 32767.0 * env * tone / 1.47;
            samples[first + i] = (int16_t)fmax(-32767.0, fmin(32767.0, mixed));
        }
    }

    FILE *f = fopen(path, "wb");
    if (!f)
        return 1;
    uint32_t bytes = (uint32_t)(total * 2);
    fwrite("RIFF", 1, 4, f);
    put32(f, 36 + bytes);
    fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16);
    put16(f, 1);
    put16(f, 1);
    put32(f, RATE);
    put32(f, RATE * 2);
    put16(f, 2);
    put16(f, 16);
    fwrite("data", 1, 4, f);
    put32(f, bytes);
    for (size_t i = 0; i < total; i++)
        put16(f, (uint16_t)samples[i]);
    return fclose(f) != 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <output dir>\n", argv[0]);
        return 2;
    }
    char path[512];
    int failed = 0;

    const note_t chime[] = { { 0.00, 0.45, 880.0, 6.0 }, { 0.16, 0.55, 1318.5, 5.0 } };
    snprintf(path, sizeof(path), "%s/chime.wav", argv[1]);
    failed |= render(path, chime, 2, 0.75, 0.55);

    note_t alarm[8];
    for (int i = 0; i < 8; i++)
        alarm[i] = (note_t){ i * 0.18, 0.14, i % 2 ? 1318.5 : 987.8, 4.0 };
    snprintf(path, sizeof(path), "%s/alarm.wav", argv[1]);
    failed |= render(path, alarm, 8, 2.0, 0.7);

    note_t timer[3];
    for (int i = 0; i < 3; i++)
        timer[i] = (note_t){ i * 0.22, 0.12, 1760.0, 3.0 };
    snprintf(path, sizeof(path), "%s/timer.wav", argv[1]);
    failed |= render(path, timer, 3, 1.4, 0.6);
    return failed;
}
