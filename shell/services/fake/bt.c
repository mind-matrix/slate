#include <stdio.h>
#include <string.h>
#include "bt.h"

static bool powered = true;

static bt_device_t devices[] = {
    { .path = "/fake/pico", .name = "Pico", .icon = "audio-card", .named = true, .paired = true, .connected = true, .rssi = -40 },
    { .path = "/fake/desk", .name = "Desk headset", .icon = "audio-headset", .named = true, .rssi = -55 },
    { .path = "/fake/living", .name = "Living room speaker", .icon = "audio-card", .named = true, .rssi = -70 },
};

bool bt_available(void)
{
    return true;
}

bool bt_powered(void)
{
    return powered;
}

bool bt_set_powered(bool on)
{
    powered = on;
    return true;
}

bool bt_alias(char *out, size_t len)
{
    snprintf(out, len, "slate");
    return true;
}

void bt_set_discovering(bool on)
{
    (void)on;
}

size_t bt_list(bt_device_t *out, size_t max)
{
    size_t n = 0;
    for (size_t i = 0; i < sizeof(devices) / sizeof(devices[0]) && n < max && powered; i++)
        out[n++] = devices[i];
    return bt_sort_devices(out, n);
}

bool bt_connected_name(char *out, size_t len)
{
    snprintf(out, len, "%s", devices[0].name);
    return powered && devices[0].connected;
}

void bt_pair(const char *path)
{
    (void)path;
}

void bt_connect(const char *path)
{
    (void)path;
}

void bt_disconnect(const char *path)
{
    (void)path;
}

void bt_forget(const char *path)
{
    (void)path;
}

bt_activity_t bt_activity(const char *path)
{
    (void)path;
    return BT_IDLE;
}

void bt_agent_start(bt_confirm_cb_t cb, void *user)
{
    (void)cb;
    (void)user;
}

void bt_agent_reply(bool accept)
{
    (void)accept;
}
