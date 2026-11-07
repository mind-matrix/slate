#include <string.h>

#include "audio.h"
#include "check.h"

static const char status[] =
    "PipeWire 'pipewire-0' [1.4.2, marvin@slate, cookie:1636227569]\n"
    " └─ Clients:\n"
    "        33. WirePlumber                         [1.4.2, marvin@slate, pid:766]\n"
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

int main(void)
{
    test_parse_volume();
    test_parse_status();
    test_kind_label();
    return failures ? 1 : 0;
}
