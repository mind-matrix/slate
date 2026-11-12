#include "bt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *prefix;
    const char *label;
} kind_t;

static const kind_t kinds[] = {
    { "audio-headset", "Headset" },
    { "audio-headphones", "Headphones" },
    { "audio-card", "Speaker" },
    { "input-keyboard", "Keyboard" },
    { "input-mouse", "Mouse" },
    { "input-gaming", "Game controller" },
    { "input-tablet", "Tablet" },
    { "phone", "Phone" },
    { "computer", "Computer" },
    { "camera", "Camera" },
    { "printer", "Printer" },
};

const char *bt_kind_label(const char *icon)
{
    for (size_t i = 0; icon && i < sizeof(kinds) / sizeof(kinds[0]); i++)
        if (strncmp(icon, kinds[i].prefix, strlen(kinds[i].prefix)) == 0)
            return kinds[i].label;
    return "Device";
}

void bt_format_passkey(uint32_t passkey, char *out, size_t len)
{
    snprintf(out, len, "%03u %03u", (passkey / 1000) % 1000, passkey % 1000);
}

static int by_signal(const void *a, const void *b)
{
    const bt_device_t *x = a, *y = b;
    if (x->paired != y->paired)
        return x->paired ? -1 : 1;
    if (x->connected != y->connected)
        return x->connected ? -1 : 1;
    if (x->rssi != y->rssi)
        return (int)y->rssi - (int)x->rssi;
    return strcmp(x->name, y->name);
}

size_t bt_sort_devices(bt_device_t *devices, size_t count)
{
    size_t kept = 0;
    for (size_t i = 0; i < count; i++)
        if (devices[i].paired || (devices[i].named && devices[i].name[0]))
            devices[kept++] = devices[i];
    qsort(devices, kept, sizeof(*devices), by_signal);
    return kept;
}

bool bt_path_has_address(const char *path, const char *address)
{
    const char *dev = strstr(path, "dev_");
    if (!dev || !address[0])
        return false;
    dev += 4;
    size_t i = 0;
    for (; address[i] && dev[i]; i++)
        if ((address[i] == ':' ? '_' : address[i]) != dev[i])
            return false;
    return !address[i] && !dev[i];
}
