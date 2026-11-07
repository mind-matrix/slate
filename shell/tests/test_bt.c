#include <string.h>

#include "bt.h"
#include "check.h"

static void test_kind(void)
{
    CHECK(strcmp(bt_kind_label("audio-headset"), "Headset") == 0);
    CHECK(strcmp(bt_kind_label("audio-card"), "Speaker") == 0);
    CHECK(strcmp(bt_kind_label("input-keyboard"), "Keyboard") == 0);
    CHECK(strcmp(bt_kind_label("phone"), "Phone") == 0);
    CHECK(strcmp(bt_kind_label(""), "Device") == 0);
    CHECK(strcmp(bt_kind_label(NULL), "Device") == 0);
}

static void test_passkey(void)
{
    char out[16];
    bt_format_passkey(492813, out, sizeof(out));
    CHECK(strcmp(out, "492 813") == 0);
    bt_format_passkey(7, out, sizeof(out));
    CHECK(strcmp(out, "000 007") == 0);
}

static void test_sort(void)
{
    bt_device_t devices[] = {
        { .name = "Far", .named = true, .rssi = -80 },
        { .name = "AA-BB-CC", .named = false, .rssi = -30 },
        { .name = "Pico", .named = true, .paired = true, .rssi = -60 },
        { .name = "Near", .named = true, .rssi = -40 },
    };
    size_t count = bt_sort_devices(devices, 4);
    CHECK(count == 3);
    CHECK(strcmp(devices[0].name, "Pico") == 0);
    CHECK(strcmp(devices[1].name, "Near") == 0);
    CHECK(strcmp(devices[2].name, "Far") == 0);
}

int main(void)
{
    test_kind();
    test_passkey();
    test_sort();
    return failures ? 1 : 0;
}
