#include "platform/backend.h"

#include <linux/input.h>

#define KEY_REPEAT 2

static const char plain_keys[] = "\0\0" "1234567890-=" "\0\0" "qwertyuiop[]" "\0\0" "asdfghjkl;'`" "\0" "\\zxcvbnm,./" "\0" "*" "\0" " ";
static const char shift_keys[] = "\0\0" "!@#$%^&*()_+" "\0\0" "QWERTYUIOP{}" "\0\0" "ASDFGHJKL:\"~" "\0" "|ZXCVBNM<>?" "\0" "*" "\0" " ";

typedef struct {
    bool shift;
    bool ctrl;
    bool alt;
    bool caps;
} keyboards_t;

static keyboards_t keyboards;

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

void sui_keyboard_key(unsigned int code, int value)
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

