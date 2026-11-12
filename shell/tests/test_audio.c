#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "audio.h"
#include "check.h"

static const char status[] =
    "PipeWire 'pipewire-0' [1.4.2, slate@slate, cookie:1636227569]\n"
    " └─ Clients:\n"
    "        33. WirePlumber                         [1.4.2, slate@slate, pid:766]\n"
    "\n"
    "Audio\n"
    " ├─ Devices:\n"
    " │      42. Built-in Audio                      [alsa]\n"
    " │      61. Pico                                [bluez5]\n"
    " │  \n"
    " ├─ Sinks:\n"
    " │  *   48. Pico                                [vol: 0.44]\n"
    " │      49. Built-in Audio Digital Stereo (HDMI) [vol: 1.00 MUTED]\n"
    " │  \n"
    " ├─ Sources:\n"
    " │  *   50. Pico                                [vol: 1.00]\n"
    " │  \n"
    " ├─ Filters:\n"
    " │  \n"
    " └─ Streams:\n"
    "        70. Firefox\n"
    "\n"
    "Video\n"
    " ├─ Devices:\n"
    " │      50. rpi-hevc-dec                        [v4l2]\n"
    "\n"
    "Settings\n"
    " └─ Default Configured Devices:\n"
    "         0. Audio/Sink    bluez_output.AA_BB.1\n";

static const char filters[] =
    "Audio\n"
    " ├─ Sinks:\n"
    " │  *   62. Built-in Audio                      [vol: 0.40]\n"
    " │  \n"
    " ├─ Filters:\n"
    " │    - loopback-1955-18\n"
    " │      69. input.loopback.sink.role.multimedia                          [Audio/Sink]\n"
    " │      77. output.loopback.sink.role.multimedia                         [Stream/Output/Audio]\n"
    " │    - loopback-1955-20\n"
    " │      70. input.loopback.sink.role.alarm                               [Audio/Sink]\n"
    " │      81. output.loopback.sink.role.alarm                              [Stream/Output/Audio]\n"
    " │    - loopback-1955-19\n"
    " │      79. input.loopback.sink.role.notification                        [Audio/Sink]\n"
    " │      80. output.loopback.sink.role.notification                       [Stream/Output/Audio]\n"
    " │  \n"
    " └─ Streams:\n";

static void test_parse_volume(void)
{
    int percent = -1;
    bool muted = true;
    CHECK(audio_parse_volume("Volume: 0.44\n", &percent, &muted));
    CHECK(percent == 44 && !muted);
    CHECK(audio_parse_volume("Volume: 1.00 [MUTED]\n", &percent, &muted));
    CHECK(percent == 100 && muted);
    CHECK(audio_parse_volume("Volume: 0.005", &percent, &muted));
    CHECK(percent == 1);
    CHECK(!audio_parse_volume("Translate ID error", &percent, &muted));
    CHECK(!audio_parse_volume("", &percent, &muted));
}

static void test_parse_status(void)
{
    audio_device_t devices[4];
    size_t count = audio_parse_status(status, AUDIO_OUTPUT, devices, 4);
    CHECK(count == 2);
    CHECK(devices[0].id == 48 && strcmp(devices[0].name, "Pico") == 0 && devices[0].is_default);
    CHECK(devices[1].id == 49 && strcmp(devices[1].name, "Built-in Audio Digital Stereo (HDMI)") == 0);
    CHECK(!devices[1].is_default);

    count = audio_parse_status(status, AUDIO_INPUT, devices, 4);
    CHECK(count == 1);
    CHECK(devices[0].id == 50 && devices[0].is_default);

    CHECK(audio_parse_status(status, AUDIO_OUTPUT, devices, 1) == 1);
    CHECK(audio_parse_status("Audio\n ├─ Sinks:\n │  \n", AUDIO_OUTPUT, devices, 4) == 0);
}

static void test_kind_label(void)
{
    CHECK(strcmp(audio_kind_label("  device.api = \"bluez5\"\n", AUDIO_OUTPUT), "Bluetooth speaker") == 0);
    CHECK(strcmp(audio_kind_label("node.name = \"alsa_output.platform-107c701400.hdmi.hdmi-stereo\"", AUDIO_OUTPUT),
                 "Display audio") == 0);
    CHECK(strcmp(audio_kind_label("node.name = \"alsa_input.usb-0d8c_USB_Sound-00.mono\"", AUDIO_INPUT),
                 "USB microphone") == 0);
    CHECK(strcmp(audio_kind_label("device.api = \"alsa\"", AUDIO_OUTPUT), "Built-in audio") == 0);
}

static void test_roles(void)
{
    uint32_t ids[AUDIO_ROLE_COUNT];
    CHECK(audio_parse_role_ids(filters, ids));
    CHECK(ids[AUDIO_ROLE_MEDIA] == 69 && ids[AUDIO_ROLE_ALARM] == 70 && ids[AUDIO_ROLE_NOTIFICATION] == 79);
    CHECK(!audio_parse_role_ids(status, ids));
    CHECK(ids[AUDIO_ROLE_MEDIA] == 0);

    audio_device_t sinks[AUDIO_MAX_DEVICES];
    CHECK(audio_parse_status(filters, AUDIO_OUTPUT, sinks, AUDIO_MAX_DEVICES) == 1);
    CHECK(sinks[0].id == 62);
    CHECK(strcmp(audio_role_media(AUDIO_ROLE_ALARM), "Alarm") == 0);
}

static void test_level_defaults(void)
{
    char home[] = "/tmp/slate-audio-XXXXXX";
    CHECK(mkdtemp(home) != NULL);
    setenv("HOME", home, 1);
    CHECK(audio_level(AUDIO_LEVEL_MEDIA) == 100);
    CHECK(audio_level(AUDIO_LEVEL_ALARM) == 100);
    CHECK(audio_level(AUDIO_LEVEL_NOTIFICATION) == 70);
    CHECK(audio_level(AUDIO_LEVEL_DUCK) == 30);
    CHECK(audio_level("unknown") == 0);
    rmdir(home);
}

static const char headset[] =
    "Audio\n"
    " ├─ Sources:\n"
    " │      108. vivo Buds                           [vol: 1.00]\n"
    " │  \n"
    " ├─ Filters:\n"
    " │    - loopback-792-22\n"
    " │  *   106. bluez_input.20:A3:66:78:0D:2E                                [Audio/Source]\n"
    " │      107. bluez_capture_internal.20:A3:66:78:0D:2E                     [Stream/Input/Audio/Internal]\n"
    " │      70. input.loopback.sink.role.multimedia                           [Audio/Sink]\n"
    " │  \n"
    " └─ Streams:\n";

static void test_headset_sources(void)
{
    audio_device_t found[AUDIO_MAX_DEVICES];
    CHECK(audio_parse_status(headset, AUDIO_INPUT, found, AUDIO_MAX_DEVICES) == 2);
    CHECK(found[0].id == 108 && strcmp(found[0].name, "vivo Buds") == 0);
    CHECK(found[1].id == 106 && found[1].is_default);
    CHECK(strcmp(found[1].name, "bluez_input.20:A3:66:78:0D:2E") == 0);
    CHECK(audio_parse_status(headset, AUDIO_OUTPUT, found, AUDIO_MAX_DEVICES) == 0);
}

static void test_inspect(void)
{
    audio_device_t loopback = { .id = 106, .name = "bluez_input.20:A3:66:78:0D:2E" };
    audio_parse_inspect("id 106, type PipeWire:Interface:Node\n"
                        "    bluez5.loopback = \"true\"\n"
                        "  * media.class = \"Audio/Source\"\n"
                        "  * node.description = \"vivo Buds\"\n"
                        "  * node.name = \"bluez_input.20:A3:66:78:0D:2E\"\n",
                        &loopback);
    CHECK(strcmp(loopback.name, "vivo Buds") == 0);
    CHECK(strcmp(loopback.address, "20:A3:66:78:0D:2E") == 0);
    CHECK(loopback.loopback && !loopback.internal && !loopback.music);

    audio_device_t sink = { .id = 115 };
    audio_parse_inspect("    api.bluez5.address = \"20:A3:66:78:0D:2E\"\n"
                        "    api.bluez5.profile = \"a2dp-sink\"\n"
                        "    bluez5.loopback = \"false\"\n"
                        "  * node.description = \"vivo Buds\"\n"
                        "  * node.name = \"bluez_output.20_A3_66_78_0D_2E.1\"\n",
                        &sink);
    CHECK(sink.music && !sink.loopback && strcmp(sink.address, "20:A3:66:78:0D:2E") == 0);

    audio_device_t internal = { .id = 108 };
    audio_parse_inspect("    api.bluez5.address = \"20:A3:66:78:0D:2E\"\n"
                        "  * node.name = \"bluez_input_internal.20_A3_66_78_0D_2E.0\"\n",
                        &internal);
    CHECK(internal.internal);

    audio_device_t builtin = { .id = 62, .name = "Built-in Audio" };
    audio_parse_inspect("  * node.name = \"alsa_output.platform\"\n", &builtin);
    CHECK(strcmp(builtin.name, "Built-in Audio") == 0 && !builtin.address[0]);
}

static void test_detail(void)
{
    char out[96];
    audio_device_t music = { .address = "20:A3:66:78:0D:2E", .music = true };
    audio_device_detail(&music, AUDIO_OUTPUT, "Headset", out, sizeof(out));
    CHECK(strcmp(out, "Bluetooth headset · music quality") == 0);
    audio_device_t call = { .address = "20:A3:66:78:0D:2E" };
    audio_device_detail(&call, AUDIO_OUTPUT, "Speaker", out, sizeof(out));
    CHECK(strcmp(out, "Bluetooth speaker · call quality") == 0);
    audio_device_t mic = { .address = "20:A3:66:78:0D:2E", .loopback = true };
    audio_device_detail(&mic, AUDIO_INPUT, "Headset", out, sizeof(out));
    CHECK(strcmp(out, "Bluetooth headset · switches to call quality while the mic is in use") == 0);
    audio_device_detail(&music, AUDIO_OUTPUT, NULL, out, sizeof(out));
    CHECK(strcmp(out, "Bluetooth speaker · music quality") == 0);
    audio_device_t hdmi = { .kind = "Display audio" };
    audio_device_detail(&hdmi, AUDIO_OUTPUT, NULL, out, sizeof(out));
    CHECK(strcmp(out, "Display audio") == 0);
}

int main(void)
{
    test_headset_sources();
    test_inspect();
    test_detail();
    test_roles();
    test_level_defaults();
    test_parse_volume();
    test_parse_status();
    test_kind_label();
    return failures ? 1 : 0;
}
