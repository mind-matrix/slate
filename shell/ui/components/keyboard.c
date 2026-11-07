#include "components/keyboard.h"

#include "theme/theme.h"

#define KEYBOARD_TEXT_H 300
#define KEYBOARD_TERMINAL_H 372
#define KEY_GAP 10
#define HOME_ROW_INDENT 52
#define SPECIAL_GROW 3
#define SPACE_GROW 12
#define CHAR_GROW 2

#define KB_SHIFT 0xE101
#define KB_LETTERS 0xE102
#define KB_SYMBOLS 0xE103
#define KB_MORE 0xE104
#define KB_CTRL 0xE105

enum { LAYER_LOWER, LAYER_UPPER, LAYER_SYMBOLS, LAYER_MORE };

typedef struct {
    sui_keyboard_mode_t mode;
    sui_key_cb_t cb;
    void *user;
    int layer;
    bool ctrl;
    lv_obj_t *obj;
    lv_obj_t *ctrl_key;
} keyboard_t;

static const char *const letter_rows[] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
static const char *const symbol_rows[] = { "1234567890", "-/:;()$&@\"", ".,?!'" };
static const char *const more_rows[] = { "[]{}#%^*+=", "_\\|~<>`", ".,?!'" };

static void build(keyboard_t *kb);

static void rebuild(void *user)
{
    keyboard_t *kb = user;
    lv_obj_clean(kb->obj);
    build(kb);
}

static void set_layer(keyboard_t *kb, int layer)
{
    kb->layer = layer;
    lv_async_call_cancel(rebuild, kb);
    lv_async_call(rebuild, kb);
}

static void set_ctrl(keyboard_t *kb, bool on)
{
    kb->ctrl = on;
    if (!kb->ctrl_key)
        return;
    if (on)
        lv_obj_add_state(kb->ctrl_key, LV_STATE_CHECKED);
    else
        lv_obj_remove_state(kb->ctrl_key, LV_STATE_CHECKED);
}

static void press(lv_event_t *e)
{
    keyboard_t *kb = lv_event_get_user_data(e);
    uint32_t code = (uint32_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_current_target(e));

    switch (code) {
    case KB_SHIFT:
        set_layer(kb, kb->layer == LAYER_UPPER ? LAYER_LOWER : LAYER_UPPER);
        return;
    case KB_LETTERS:
        set_layer(kb, LAYER_LOWER);
        return;
    case KB_SYMBOLS:
        set_layer(kb, LAYER_SYMBOLS);
        return;
    case KB_MORE:
        set_layer(kb, LAYER_MORE);
        return;
    case KB_CTRL:
        set_ctrl(kb, !kb->ctrl);
        return;
    }

    uint32_t mods = kb->ctrl ? SUI_MOD_CTRL : 0;
    set_ctrl(kb, false);
    if (kb->layer == LAYER_UPPER && code >= 'A' && code <= 'Z')
        set_layer(kb, LAYER_LOWER);
    kb->cb(code, mods, kb->user);
}

static lv_obj_t *add_row(keyboard_t *kb, int32_t indent)
{
    lv_obj_t *row = sui_box_create(kb->obj);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_flex_grow(row, 1);
    lv_obj_set_style_pad_hor(row, indent, 0);
    lv_obj_set_style_pad_column(row, KEY_GAP, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    return row;
}

static lv_obj_t *add_key(keyboard_t *kb, lv_obj_t *row, const char *label, uint32_t code, uint8_t grow, bool special)
{
    lv_obj_t *key = sui_box_create(row);
    lv_obj_add_flag(key, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(key, LV_PCT(100));
    lv_obj_set_flex_grow(key, grow);
    lv_obj_set_user_data(key, (void *)(uintptr_t)code);
    lv_obj_set_style_radius(key, SUI_RADIUS_BUTTON, 0);
    lv_obj_set_style_bg_opa(key, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(key, lv_color_hex(special ? SUI_COLOR_BORDER : SUI_COLOR_RAISED), 0);
    lv_obj_set_style_bg_color(key, lv_color_hex(SUI_COLOR_ACCENT), LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(key, lv_color_hex(SUI_COLOR_TRACK), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(key, lv_color_hex(SUI_COLOR_TEXT), 0);
    lv_obj_set_style_text_color(key, lv_color_hex(SUI_COLOR_ON_ACCENT), LV_STATE_CHECKED);
    lv_obj_set_style_text_font(key, sui_font(special ? SUI_TEXT_ROW : SUI_TEXT_HEADING), 0);
    lv_obj_add_event_cb(key, press, LV_EVENT_CLICKED, kb);

    lv_obj_t *text = lv_label_create(key);
    lv_label_set_text(text, label);
    lv_obj_center(text);
    return key;
}

static void add_chars(keyboard_t *kb, lv_obj_t *row, const char *chars)
{
    for (const char *c = chars; *c; c++) {
        char ch = kb->layer == LAYER_UPPER && *c >= 'a' && *c <= 'z' ? (char)(*c - 'a' + 'A') : *c;
        char label[2] = { ch, '\0' };
        add_key(kb, row, label, (uint32_t)(unsigned char)ch, CHAR_GROW, false);
    }
}

static void build_terminal_row(keyboard_t *kb)
{
    lv_obj_t *row = add_row(kb, 0);
    add_key(kb, row, "Esc", SUI_KEY_ESC, SPECIAL_GROW, true);
    add_key(kb, row, "Tab", SUI_KEY_TAB, SPECIAL_GROW, true);
    kb->ctrl_key = add_key(kb, row, "Ctrl", KB_CTRL, SPECIAL_GROW, true);
    set_ctrl(kb, kb->ctrl);
    add_key(kb, row, LV_SYMBOL_LEFT, SUI_KEY_LEFT, SPECIAL_GROW, true);
    add_key(kb, row, LV_SYMBOL_UP, SUI_KEY_UP, SPECIAL_GROW, true);
    add_key(kb, row, LV_SYMBOL_DOWN, SUI_KEY_DOWN, SPECIAL_GROW, true);
    add_key(kb, row, LV_SYMBOL_RIGHT, SUI_KEY_RIGHT, SPECIAL_GROW, true);
}

static void build(keyboard_t *kb)
{
    bool letters = kb->layer == LAYER_LOWER || kb->layer == LAYER_UPPER;
    const char *const *rows = letters ? letter_rows : kb->layer == LAYER_SYMBOLS ? symbol_rows : more_rows;

    kb->ctrl_key = NULL;
    if (kb->mode == SUI_KEYBOARD_TERMINAL)
        build_terminal_row(kb);

    add_chars(kb, add_row(kb, 0), rows[0]);
    add_chars(kb, add_row(kb, letters ? HOME_ROW_INDENT : 0), rows[1]);

    lv_obj_t *row = add_row(kb, 0);
    if (letters) {
        lv_obj_t *shift = add_key(kb, row, LV_SYMBOL_UP, KB_SHIFT, SPECIAL_GROW, true);
        if (kb->layer == LAYER_UPPER)
            lv_obj_add_state(shift, LV_STATE_CHECKED);
    } else {
        bool symbols = kb->layer == LAYER_SYMBOLS;
        add_key(kb, row, symbols ? "#+=" : "123", symbols ? KB_MORE : KB_SYMBOLS, SPECIAL_GROW, true);
    }
    add_chars(kb, row, rows[2]);
    add_key(kb, row, LV_SYMBOL_BACKSPACE, SUI_KEY_BACKSPACE, SPECIAL_GROW, true);

    row = add_row(kb, 0);
    add_key(kb, row, letters ? "123" : "ABC", letters ? KB_SYMBOLS : KB_LETTERS, SPECIAL_GROW, true);
    add_key(kb, row, "space", ' ', SPACE_GROW, true);
    if (kb->mode == SUI_KEYBOARD_TERMINAL)
        add_key(kb, row, "Enter", SUI_KEY_ENTER, SPECIAL_GROW, true);
    else
        add_key(kb, row, ".", '.', CHAR_GROW, true);
    add_key(kb, row, LV_SYMBOL_DOWN, SUI_KEY_HIDE, SPECIAL_GROW, true);
}

static void on_delete(lv_event_t *e)
{
    keyboard_t *kb = lv_event_get_user_data(e);
    lv_async_call_cancel(rebuild, kb);
    lv_free(kb);
}

lv_obj_t *sui_keyboard_create(lv_obj_t *parent, sui_keyboard_mode_t mode, sui_key_cb_t cb, void *user)
{
    keyboard_t *kb = lv_malloc_zeroed(sizeof(*kb));
    kb->mode = mode;
    kb->cb = cb;
    kb->user = user;
    kb->layer = LAYER_LOWER;

    kb->obj = sui_box_create(parent);
    lv_obj_add_flag(kb->obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(kb->obj, LV_PCT(100), mode == SUI_KEYBOARD_TERMINAL ? KEYBOARD_TERMINAL_H : KEYBOARD_TEXT_H);
    lv_obj_set_style_pad_top(kb->obj, 12, 0);
    lv_obj_set_style_pad_bottom(kb->obj, SUI_SPACE_M, 0);
    lv_obj_set_style_pad_hor(kb->obj, SUI_SPACE_L, 0);
    lv_obj_set_style_pad_row(kb->obj, KEY_GAP, 0);
    lv_obj_set_style_bg_color(kb->obj, lv_color_hex(SUI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(kb->obj, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(kb->obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_event_cb(kb->obj, on_delete, LV_EVENT_DELETE, kb);
    build(kb);
    return kb->obj;
}
