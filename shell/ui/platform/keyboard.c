#include "platform/backend.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define MAX_DEVICES 16
#define RESCAN_MS 2000
#define LONG_BITS (8 * sizeof(long))
#define BIT_WORDS(n) (((n) + LONG_BITS - 1) / LONG_BITS)
#define KEY_REPEAT 2

static const char plain_keys[] = "\0\0" "1234567890-=" "\0\0" "qwertyuiop[]" "\0\0" "asdfghjkl;'`" "\0" "\\zxcvbnm,./" "\0" "*" "\0" " ";
static const char shift_keys[] = "\0\0" "!@#$%^&*()_+" "\0\0" "QWERTYUIOP{}" "\0\0" "ASDFGHJKL:\"~" "\0" "|ZXCVBNM<>?" "\0" "*" "\0" " ";

typedef struct {
    int fds[MAX_DEVICES];
    bool shift;
    bool ctrl;
    bool alt;
    bool caps;
} keyboards_t;

static keyboards_t keyboards;

static bool has_bit(const unsigned long *bits, unsigned int bit)
{
    return (bits[bit / LONG_BITS] >> (bit % LONG_BITS)) & 1UL;
}

static bool is_keyboard(int fd)
{
    unsigned long bits[BIT_WORDS(KEY_CNT)] = { 0 };
    return ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) >= 0 && has_bit(bits, KEY_A) && has_bit(bits, KEY_SPACE);
}

static uint32_t special_key(unsigned int code)
{
    switch (code) {
    case KEY_ENTER:
    case KEY_KPENTER:
        return SUI_KEY_ENTER;
    case KEY_BACKSPACE:
        return SUI_KEY_BACKSPACE;
    case KEY_TAB:
        return SUI_KEY_TAB;
    case KEY_ESC:
        return SUI_KEY_ESC;
    case KEY_UP:
        return SUI_KEY_UP;
    case KEY_DOWN:
        return SUI_KEY_DOWN;
    case KEY_LEFT:
        return SUI_KEY_LEFT;
    case KEY_RIGHT:
        return SUI_KEY_RIGHT;
    case KEY_HOME:
        return SUI_KEY_HOME;
    case KEY_END:
        return SUI_KEY_END;
    case KEY_PAGEUP:
        return SUI_KEY_PAGE_UP;
    case KEY_PAGEDOWN:
        return SUI_KEY_PAGE_DOWN;
    case KEY_DELETE:
        return SUI_KEY_DELETE;
    default:
        return 0;
    }
}

static uint32_t text_key(unsigned int code)
{
    if (code >= sizeof(plain_keys) - 1)
        return 0;
    char c = keyboards.shift ? shift_keys[code] : plain_keys[code];
    if (keyboards.caps && c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');
    else if (keyboards.caps && c >= 'A' && c <= 'Z')
        c = (char)(c - 'A' + 'a');
    return (uint32_t)(unsigned char)c;
}

static void handle_key(unsigned int code, int value)
{
    bool down = value != 0;
    switch (code) {
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
        keyboards.shift = down;
        return;
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
        keyboards.ctrl = down;
        return;
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
        keyboards.alt = down;
        return;
    case KEY_CAPSLOCK:
        if (value == 1)
            keyboards.caps = !keyboards.caps;
        return;
    }
    if (value != 1 && value != KEY_REPEAT)
        return;

    uint32_t key = special_key(code);
    if (!key)
        key = text_key(code);
    if (!key)
        return;
    uint32_t mods = (keyboards.ctrl ? SUI_MOD_CTRL : 0) | (keyboards.alt ? SUI_MOD_ALT : 0);
    sui_key_dispatch(key, mods);
}

static void close_device(int slot)
{
    sui_unwatch_fd(keyboards.fds[slot]);
    close(keyboards.fds[slot]);
    keyboards.fds[slot] = -1;
}

static void read_device(int fd, void *user)
{
    int slot = (int)(intptr_t)user;
    struct input_event ev;
    ssize_t n;

    while ((n = read(fd, &ev, sizeof(ev))) == (ssize_t)sizeof(ev))
        if (ev.type == EV_KEY)
            handle_key(ev.code, ev.value);
    if (n == 0 || (n < 0 && errno != EAGAIN))
        close_device(slot);
}

static void scan(lv_timer_t *timer)
{
    (void)timer;
    char path[32];
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (keyboards.fds[i] >= 0)
            continue;
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
            continue;
        if (is_keyboard(fd) && sui_watch_fd(fd, read_device, (void *)(intptr_t)i))
            keyboards.fds[i] = fd;
        else
            close(fd);
    }
}

void sui_keyboard_open(void)
{
    for (int i = 0; i < MAX_DEVICES; i++)
        keyboards.fds[i] = -1;
    scan(NULL);
    lv_timer_create(scan, RESCAN_MS, NULL);
}
