#include "platform/backend.h"

#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define LONG_BITS (8 * sizeof(long))
#define BIT_WORDS(n) (((n) + LONG_BITS - 1) / LONG_BITS)

typedef struct {
    int fd;
    int32_t x;
    int32_t y;
    bool pressed;
} touch_t;

static touch_t touch = { .fd = -1 };

static bool has_bit(const unsigned long *bits, unsigned int bit)
{
    return (bits[bit / LONG_BITS] >> (bit % LONG_BITS)) & 1UL;
}

static bool is_touchscreen(const char *path)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return false;

    unsigned long bits[BIT_WORDS(ABS_CNT)] = { 0 };
    bool found = ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(bits)), bits) >= 0 && has_bit(bits, ABS_MT_POSITION_X);
    close(fd);
    return found;
}

static void read_touch(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    struct input_event ev;

    while (read(touch.fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        if (ev.type == EV_ABS && ev.code == ABS_X)
            touch.x = ev.value;
        else if (ev.type == EV_ABS && ev.code == ABS_Y)
            touch.y = ev.value;
        else if (ev.type == EV_KEY && ev.code == BTN_TOUCH)
            touch.pressed = ev.value != 0;
    }

    data->point.x = touch.x;
    data->point.y = touch.y;
    data->state = touch.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

lv_indev_t *sui_touch_open(lv_display_t *disp)
{
    const char *path = getenv("SLATE_TOUCH_DEVICE");
    char candidate[32];

    for (int i = 0; !path && i < 16; i++) {
        snprintf(candidate, sizeof(candidate), "/dev/input/event%d", i);
        if (is_touchscreen(candidate))
            path = candidate;
    }
    if (!path)
        return NULL;

    touch.fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (touch.fd < 0)
        return NULL;

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, read_touch);
    lv_indev_set_display(indev, disp);
    return indev;
}
