#include <stdio.h>
#include <string.h>

#include "apps.h"

static const app_t fake_apps[] = {
    { .id = "ava", .name = "Ava", .version = "0.9.4-dev",
      .requested = "audio network discovery secrets assistant gui overlay notifications wifi bluetooth power timezone",
      .granted = "audio network discovery secrets assistant gui overlay notifications wifi bluetooth power timezone",
      .registration = true, .dev = true },
    { .id = "hello", .name = "Hello", .version = "0.1.0", .requested = "network", .granted = "network" },
    { .id = "marvin", .name = "Marvin", .version = "1.2.0",
      .requested = "audio network discovery secrets assistant gui overlay notifications wifi bluetooth power timezone",
      .granted = "audio network discovery secrets assistant gui overlay notifications wifi bluetooth power timezone" },
};

static const struct {
    const char *name;
    const char *title;
    const char *detail;
} permissions[] = {
    { "audio", "Microphone and speaker", "Only while it's the selected assistant" },
    { "network", "Network", "Internet and local network" },
    { "discovery", "Device discovery", "Find and register nearby devices" },
    { "secrets", "Secure storage", "Its own private secrets" },
    { "assistant", "Voice assistant", "Can be selected as the assistant" },
    { "gui", "Full-screen app", "Its own screens, with Back and Home" },
    { "overlay", "Draw over other apps", "Voice captions and controls" },
    { "notifications", "Notifications", "Post to the notification list" },
    { "wifi", "Wi-Fi", "Join and change networks" },
    { "bluetooth", "Bluetooth", "Pair and connect devices" },
    { "power", "Power", "Restart and shut down" },
    { "timezone", "Time zone", "Change the time zone" },
};

size_t apps_list(app_t *out, size_t max)
{
    size_t count = 0;
    for (; count < sizeof(fake_apps) / sizeof(fake_apps[0]) && count < max; count++)
        out[count] = fake_apps[count];
    apps_sort(out, count);
    return count;
}

bool apps_permission_text(const char *permission, char *title, size_t title_len, char *detail, size_t detail_len)
{
    for (size_t i = 0; i < sizeof(permissions) / sizeof(permissions[0]); i++) {
        if (strcmp(permissions[i].name, permission) == 0) {
            snprintf(title, title_len, "%s", permissions[i].title);
            snprintf(detail, detail_len, "%s", permissions[i].detail);
            return true;
        }
    }
    return false;
}

/* Apps run until stopped; the fake remembers which ones were stopped. */
static char stopped[APPS_MAX][APPS_ID_MAX];

static int stopped_slot(const char *id)
{
    for (int i = 0; i < APPS_MAX; i++)
        if (strcmp(stopped[i], id) == 0)
            return i;
    return -1;
}

bool apps_start(const char *id)
{
    int slot = stopped_slot(id);
    if (slot >= 0)
        stopped[slot][0] = '\0';
    return apps_valid_id(id);
}

bool apps_restart(const char *id)
{
    return apps_start(id);
}

bool apps_stop(const char *id)
{
    int slot = stopped_slot(id) >= 0 ? -1 : stopped_slot("");
    if (slot >= 0)
        snprintf(stopped[slot], sizeof(stopped[slot]), "%s", id);
    return apps_valid_id(id);
}

bool apps_running(const char *id)
{
    return apps_valid_id(id) && stopped_slot(id) < 0;
}
