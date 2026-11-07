#include <stdio.h>
#include <string.h>
#include <time.h>
#include "update.h"

#define FAKE_VERSION "2026.11.20"
#define FAKE_SIZE (18ull * 1024 * 1024)
#define PROGRESS_STEP 9

static update_status_t fake = {
    .state = UPDATE_AVAILABLE,
    .version = FAKE_VERSION,
    .size = FAKE_SIZE,
};

static const char *const notes[] = {
    "Wi-Fi: hidden networks can be joined from the list.",
    "Fixes a case where the drawer did not close after pairing.",
};

void update_read(update_status_t *out)
{
    if (fake.state == UPDATE_CHECKING)
        fake.state = UPDATE_AVAILABLE;
    else if (fake.state == UPDATE_DOWNLOADING && (fake.progress += PROGRESS_STEP) >= 100)
        fake.state = UPDATE_INSTALLING;
    else if (fake.state == UPDATE_INSTALLING)
        fake.state = UPDATE_RESTART;
    fake.checked = (int64_t)time(NULL);
    *out = fake;
}

size_t update_read_notes(bool available, update_note_t *out, size_t max)
{
    (void)available;
    size_t count = 0;
    for (; count < sizeof(notes) / sizeof(notes[0]) && count < max; count++)
        snprintf(out[count], UPDATE_NOTE_MAX, "%s", notes[count]);
    return count;
}

bool update_start_check(void)
{
    fake.state = UPDATE_CHECKING;
    return true;
}

bool update_start_install(void)
{
    fake.state = UPDATE_DOWNLOADING;
    fake.progress = 0;
    return true;
}

bool update_cancel(void)
{
    fake.state = UPDATE_AVAILABLE;
    return true;
}
