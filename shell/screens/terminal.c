#include "terminal.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <vterm.h>
#include "components/button.h"
#include "components/header.h"
#include "components/keyboard.h"
#include "platform/platform.h"
#include "tty.h"
#include "theme/theme.h"

#define PANEL_MARGIN 32
#define PANEL_GAP 16
#define PANEL_PAD_VER 24
#define PANEL_PAD_HOR 28
#define PANEL_RADIUS 20
#define KEYBOARD_H 372
#define MAX_COLS 160
#define MAX_ROWS 64
#define READ_CHUNK 4096
#define UTF8_MAX 4
#define HEADER_PAD_RIGHT 32

static const uint32_t palette[16] = {
    0x3D4A72, 0xFF8A7A, 0x5FD39A, 0xFFC65C, 0x7AB0FF, 0xA99BFF, 0x5FD3C4, 0xA9B3CC,
    0x6A769A, 0xFFB0A6, 0x8AE6B8, 0xFFD98F, 0xA8CBFF, 0xC9BFFF, 0x8FE6DA, 0xFFFFFF,
};

typedef struct {
    VTerm *vt;
    VTermScreen *screen;
    int fd;
    pid_t pid;
    int rows;
    int cols;
    int cell_w;
    int cell_h;
    VTermPos cursor;
    bool cursor_visible;
    lv_obj_t *root;
    lv_obj_t *panel;
    lv_obj_t *keyboard;
} terminal_t;

static terminal_t term = { .fd = -1 };

static void invalidate_rows(int first, int last)
{
    if (!term.panel)
        return;
    lv_area_t area;
    lv_obj_get_content_coords(term.panel, &area);
    int32_t top = area.y1;
    area.y1 = top + first * term.cell_h;
    area.y2 = top + (last + 1) * term.cell_h - 1;
    lv_obj_invalidate_area(term.panel, &area);
}

static int on_damage(VTermRect rect, void *user)
{
    (void)user;
    invalidate_rows(rect.start_row, rect.end_row - 1);
    return 1;
}

static int on_move_cursor(VTermPos pos, VTermPos old, int visible, void *user)
{
    (void)user;
    term.cursor = pos;
    term.cursor_visible = visible;
    invalidate_rows(old.row, old.row);
    invalidate_rows(pos.row, pos.row);
    return 1;
}

static int on_term_prop(VTermProp prop, VTermValue *value, void *user)
{
    (void)user;
    if (prop == VTERM_PROP_CURSORVISIBLE) {
        term.cursor_visible = value->boolean;
        invalidate_rows(term.cursor.row, term.cursor.row);
    }
    return 1;
}

static const VTermScreenCallbacks screen_callbacks = {
    .damage = on_damage,
    .movecursor = on_move_cursor,
    .settermprop = on_term_prop,
};

static void on_output(const char *bytes, size_t len, void *user)
{
    (void)user;
    while (len > 0 && term.fd >= 0) {
        ssize_t n = write(term.fd, bytes, len);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return;
        bytes += n;
        len -= (size_t)n;
    }
}

static lv_color_t resolve(const VTermColor *color, bool foreground, bool bold)
{
    if (foreground ? VTERM_COLOR_IS_DEFAULT_FG(color) : VTERM_COLOR_IS_DEFAULT_BG(color))
        return lv_color_hex(foreground ? SUI_COLOR_TEXT : SUI_COLOR_CONSOLE);
    VTermColor rgb = *color;
    if (VTERM_COLOR_IS_INDEXED(color)) {
        uint8_t index = color->indexed.idx;
        if (foreground && bold && index < 8)
            index += 8;
        if (index < 16)
            return lv_color_hex(palette[index]);
        vterm_screen_convert_color_to_rgb(term.screen, &rgb);
    }
    return lv_color_make(rgb.rgb.red, rgb.rgb.green, rgb.rgb.blue);
}

static size_t encode_utf8(uint32_t c, char *out)
{
    if (c < 0x80) {
        out[0] = (char)c;
        return 1;
    }
    if (c < 0x800) {
        out[0] = (char)(0xC0 | (c >> 6));
        out[1] = (char)(0x80 | (c & 0x3F));
        return 2;
    }
    if (c < 0x10000) {
        out[0] = (char)(0xE0 | (c >> 12));
        out[1] = (char)(0x80 | ((c >> 6) & 0x3F));
        out[2] = (char)(0x80 | (c & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (c >> 18));
    out[1] = (char)(0x80 | ((c >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((c >> 6) & 0x3F));
    out[3] = (char)(0x80 | (c & 0x3F));
    return 4;
}

static uint32_t printable(uint32_t c)
{
    lv_font_glyph_dsc_t glyph;
    if (c == 0)
        return ' ';
    if (c >= ' ' && c < 0x7f)
        return c;
    return lv_font_get_glyph_dsc(sui_font_mono(), &glyph, c, 0) ? c : '?';
}

typedef struct {
    lv_layer_t *layer;
    const lv_area_t *box;
    int row;
    int start;
    int end;
    lv_color_t fg;
    lv_color_t bg;
    bool ink;
    size_t used;
    char text[MAX_COLS * UTF8_MAX + 1];
} run_t;

static void flush_run(run_t *run)
{
    if (run->end <= run->start)
        return;
    lv_area_t area = {
        .x1 = run->box->x1 + run->start * term.cell_w,
        .y1 = run->box->y1 + run->row * term.cell_h,
        .x2 = run->box->x1 + run->end * term.cell_w - 1,
        .y2 = run->box->y1 + (run->row + 1) * term.cell_h - 1,
    };
    if (!lv_color_eq(run->bg, lv_color_hex(SUI_COLOR_CONSOLE))) {
        lv_draw_rect_dsc_t rect;
        lv_draw_rect_dsc_init(&rect);
        rect.bg_color = run->bg;
        lv_draw_rect(run->layer, &rect, &area);
    }
    if (run->ink) {
        run->text[run->used] = '\0';
        lv_draw_label_dsc_t label;
        lv_draw_label_dsc_init(&label);
        label.font = sui_font_mono();
        label.color = run->fg;
        label.text = run->text;
        label.text_local = 1;
        label.flag = LV_TEXT_FLAG_EXPAND;
        lv_draw_label(run->layer, &label, &area);
    }
    run->start = run->end;
    run->used = 0;
    run->ink = false;
}

static void draw_row(lv_layer_t *layer, const lv_area_t *box, int row)
{
    run_t run = { .layer = layer, .box = box, .row = row };
    for (int col = 0; col < term.cols;) {
        VTermScreenCell cell;
        vterm_screen_get_cell(term.screen, (VTermPos){ .row = row, .col = col }, &cell);
        lv_color_t fg = resolve(&cell.fg, true, cell.attrs.bold);
        lv_color_t bg = resolve(&cell.bg, false, false);
        if (cell.attrs.reverse) {
            lv_color_t swap = fg;
            fg = bg;
            bg = swap;
        }
        if (term.cursor_visible && term.cursor.row == row && term.cursor.col == col) {
            fg = lv_color_hex(SUI_COLOR_CONSOLE);
            bg = lv_color_hex(SUI_COLOR_SUCCESS);
        }
        int width = cell.width == 2 ? 2 : 1;
        if (run.end > run.start && (!lv_color_eq(fg, run.fg) || !lv_color_eq(bg, run.bg)))
            flush_run(&run);
        run.fg = fg;
        run.bg = bg;

        uint32_t c = printable(cell.chars[0]);
        run.used += encode_utf8(width == 2 ? '?' : c, run.text + run.used);
        if (width == 2)
            run.text[run.used++] = ' ';
        run.ink = run.ink || c != ' ';
        col += width;
        run.end = col;
    }
    flush_run(&run);
}

static void draw_grid(lv_event_t *e)
{
    if (!term.screen)
        return;
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t box;
    lv_obj_get_content_coords(term.panel, &box);
    int first = LV_MAX(0, (layer->_clip_area.y1 - box.y1) / term.cell_h);
    int last = LV_MIN(term.rows - 1, (layer->_clip_area.y2 - box.y1) / term.cell_h);
    for (int row = first; row <= last; row++)
        draw_row(layer, &box, row);
}

static void end_session(void)
{
    if (term.fd >= 0)
        sui_unwatch_fd(term.fd);
    tty_close(term.fd, term.pid);
    term.fd = -1;
    term.pid = 0;
    if (term.vt)
        vterm_free(term.vt);
    term.vt = NULL;
    term.screen = NULL;
}

static void leave(void *user)
{
    (void)user;
    if (sui_nav_top() == &terminal_screen)
        sui_nav_pop();
}

static void on_pty(int fd, void *user)
{
    (void)user;
    char buf[READ_CHUNK];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        vterm_input_write(term.vt, buf, (size_t)n);
    if (term.screen)
        vterm_screen_flush_damage(term.screen);
    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EINTR)) {
        end_session();
        lv_async_call(leave, NULL);
    }
}

static void fit(void)
{
    lv_obj_update_layout(term.root);
    int cols = LV_CLAMP(1, lv_obj_get_content_width(term.panel) / term.cell_w, MAX_COLS);
    int rows = LV_CLAMP(1, lv_obj_get_content_height(term.panel) / term.cell_h, MAX_ROWS);
    if (rows == term.rows && cols == term.cols)
        return;
    term.rows = rows;
    term.cols = cols;
    if (term.vt)
        vterm_set_size(term.vt, rows, cols);
    if (term.fd >= 0)
        tty_resize(term.fd, rows, cols);
    lv_obj_invalidate(term.panel);
}

static void layout_panel(void)
{
    int32_t top = SUI_STATUS_BAR_H + SUI_HEADER_H;
    int32_t bottom = term.keyboard ? KEYBOARD_H + PANEL_GAP : PANEL_MARGIN;
    lv_obj_set_pos(term.panel, PANEL_MARGIN, top);
    lv_obj_set_size(term.panel, SUI_SCREEN_W - 2 * PANEL_MARGIN, SUI_SCREEN_H - top - bottom);
    fit();
}

static void on_key(uint32_t key, uint32_t mods, void *user);

static void set_keyboard(bool visible)
{
    if (visible == (term.keyboard != NULL))
        return;
    if (visible) {
        term.keyboard = sui_keyboard_create(term.root, SUI_KEYBOARD_TERMINAL, on_key, NULL);
        lv_obj_align(term.keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    } else {
        lv_obj_delete_async(term.keyboard);
        term.keyboard = NULL;
    }
    layout_panel();
}

static VTermKey special_key(uint32_t key)
{
    switch (key) {
    case SUI_KEY_ENTER:
        return VTERM_KEY_ENTER;
    case SUI_KEY_BACKSPACE:
        return VTERM_KEY_BACKSPACE;
    case SUI_KEY_TAB:
        return VTERM_KEY_TAB;
    case SUI_KEY_ESC:
        return VTERM_KEY_ESCAPE;
    case SUI_KEY_UP:
        return VTERM_KEY_UP;
    case SUI_KEY_DOWN:
        return VTERM_KEY_DOWN;
    case SUI_KEY_LEFT:
        return VTERM_KEY_LEFT;
    case SUI_KEY_RIGHT:
        return VTERM_KEY_RIGHT;
    case SUI_KEY_HOME:
        return VTERM_KEY_HOME;
    case SUI_KEY_END:
        return VTERM_KEY_END;
    case SUI_KEY_PAGE_UP:
        return VTERM_KEY_PAGEUP;
    case SUI_KEY_PAGE_DOWN:
        return VTERM_KEY_PAGEDOWN;
    case SUI_KEY_DELETE:
        return VTERM_KEY_DEL;
    default:
        return VTERM_KEY_NONE;
    }
}

static void on_key(uint32_t key, uint32_t mods, void *user)
{
    (void)user;
    if (key == SUI_KEY_HIDE) {
        set_keyboard(false);
        return;
    }
    if (!term.vt)
        return;
    VTermModifier mod = (mods & SUI_MOD_CTRL ? VTERM_MOD_CTRL : 0) | (mods & SUI_MOD_ALT ? VTERM_MOD_ALT : 0);
    VTermKey special = special_key(key);
    if (special != VTERM_KEY_NONE)
        vterm_keyboard_key(term.vt, special, mod);
    else if (key < SUI_KEY_ENTER) {
        if ((mod & VTERM_MOD_CTRL) && key >= 'A' && key <= 'Z')
            key += 'a' - 'A';
        vterm_keyboard_unichar(term.vt, key, mod);
    }
}

static void toggle_keyboard(lv_event_t *e)
{
    (void)e;
    set_keyboard(term.keyboard == NULL);
}

static void show_keyboard(lv_event_t *e)
{
    (void)e;
    set_keyboard(true);
}

static bool start_session(void)
{
    term.vt = vterm_new(term.rows, term.cols);
    vterm_set_utf8(term.vt, 1);
    vterm_output_set_callback(term.vt, on_output, NULL);
    term.screen = vterm_obtain_screen(term.vt);
    vterm_screen_set_callbacks(term.screen, &screen_callbacks, NULL);
    vterm_screen_enable_altscreen(term.screen, 1);
    vterm_screen_reset(term.screen, 1);

    term.fd = tty_spawn(term.rows, term.cols, &term.pid);
    if (term.fd < 0 || !sui_watch_fd(term.fd, on_pty, NULL)) {
        end_session();
        return false;
    }
    return true;
}

static void on_delete(lv_event_t *e)
{
    (void)e;
    end_session();
    term = (terminal_t){ .fd = -1 };
}

static void create_header(lv_obj_t *screen)
{
    lv_obj_t *header = sui_header_create(screen, "Terminal");
    lv_obj_set_style_pad_right(header, HEADER_PAD_RIGHT, 0);
    lv_obj_t *spacer = sui_box_create(header);
    lv_obj_set_flex_grow(spacer, 1);

    lv_obj_t *button = sui_button_create(header, "Keyboard", SUI_COLOR_CAT_TERMINAL, SUI_COLOR_CAT_TERMINAL);
    lv_obj_set_style_bg_opa(button, SUI_TINT_OPA, 0);
    lv_obj_t *glyph = lv_label_create(button);
    lv_label_set_text(glyph, LV_SYMBOL_KEYBOARD);
    lv_obj_move_to_index(glyph, 0);
    lv_obj_add_event_cb(button, toggle_keyboard, LV_EVENT_CLICKED, NULL);
}

static void create(lv_obj_t *screen, void *args)
{
    (void)args;
    term = (terminal_t){ .fd = -1, .root = screen };
    term.cell_w = lv_font_get_glyph_width(sui_font_mono(), 'M', 0);
    term.cell_h = lv_font_get_line_height(sui_font_mono());
    lv_obj_add_event_cb(screen, on_delete, LV_EVENT_DELETE, NULL);
    create_header(screen);

    term.panel = sui_box_create(screen);
    lv_obj_add_flag(term.panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_ver(term.panel, PANEL_PAD_VER, 0);
    lv_obj_set_style_pad_hor(term.panel, PANEL_PAD_HOR, 0);
    lv_obj_set_style_radius(term.panel, PANEL_RADIUS, 0);
    lv_obj_set_style_bg_color(term.panel, lv_color_hex(SUI_COLOR_CONSOLE), 0);
    lv_obj_set_style_bg_opa(term.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(term.panel, lv_color_hex(SUI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(term.panel, 1, 0);
    lv_obj_add_event_cb(term.panel, draw_grid, LV_EVENT_DRAW_MAIN_END, NULL);
    lv_obj_add_event_cb(term.panel, show_keyboard, LV_EVENT_CLICKED, NULL);
    layout_panel();
    start_session();
}

static void on_show(lv_obj_t *screen)
{
    (void)screen;
    sui_set_key_handler(on_key, NULL);
}

static void on_hide(lv_obj_t *screen)
{
    (void)screen;
    sui_set_key_handler(NULL, NULL);
}

const sui_screen_def_t terminal_screen = {
    .name = "terminal",
    .create = create,
    .on_show = on_show,
    .on_hide = on_hide,
};
