#include "wifi_join.h"

#include <stdio.h>
#include <string.h>
#include "platform/windows.h"
#include "components/button.h"
#include "components/keyboard.h"
#include "platform/platform.h"
#include "theme/theme.h"

#define SCRIM_COLOR 0x080C18
#define SCRIM_OPA 140
#define SHEET_RADIUS 24
#define SHEET_PAD_TOP 20
#define FIELD_H 64
#define FIELD_PAD 20
#define FIELD_RADIUS 14
#define CURSOR_W 2
#define CURSOR_H 30
#define BULLET "•"
#define BULLET_LEN (sizeof(BULLET) - 1)
#define DISPLAY_MAX (NET_PASSWORD_MAX * BULLET_LEN + 1)
#define TEXT 96

typedef struct {
    lv_obj_t *scrim;
    lv_obj_t *sheet;
    lv_obj_t *join;
    lv_obj_t *field;
    lv_obj_t *text;
    lv_obj_t *cursor;
    lv_obj_t *reveal;
    lv_obj_t *keyboard;
    net_ap_t ap;
    wifi_join_cb_t on_join;
    char password[NET_PASSWORD_MAX];
    size_t length;
    bool revealed;
} join_sheet_t;

static join_sheet_t sheet;

static void wipe(void)
{
    memset(sheet.password, 0, sizeof(sheet.password));
    sheet.length = 0;
}

static bool ready(void)
{
    return sheet.length >= NET_PASSWORD_MIN;
}

static void render(void)
{
    char shown[DISPLAY_MAX] = "";
    if (sheet.revealed)
        snprintf(shown, sizeof(shown), "%s", sheet.password);
    else
        for (size_t i = 0; i < sheet.length; i++)
            memcpy(shown + i * BULLET_LEN, BULLET, BULLET_LEN + 1);

    bool empty = sheet.length == 0;
    lv_label_set_text(sheet.text, empty ? "Password (8+ characters)" : shown);
    lv_obj_set_style_text_color(sheet.text, lv_color_hex(empty ? SUI_COLOR_TEXT_MUTED : SUI_COLOR_TEXT), 0);
    lv_obj_set_style_text_font(sheet.text, sui_font(empty ? SUI_TEXT_ROW : SUI_TEXT_HEADING), 0);
    lv_obj_set_style_bg_color(sheet.join, lv_color_hex(ready() ? SUI_COLOR_ACCENT : SUI_COLOR_RAISED), 0);
    lv_obj_set_style_text_color(sheet.join, lv_color_hex(ready() ? SUI_COLOR_ON_ACCENT : SUI_COLOR_TEXT_MUTED), 0);

    bool typing = sheet.keyboard != NULL;
    lv_obj_set_style_border_color(sheet.field, lv_color_hex(typing ? SUI_COLOR_ACCENT : SUI_COLOR_BORDER), 0);
    if (typing)
        lv_obj_remove_flag(sheet.cursor, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(sheet.cursor, LV_OBJ_FLAG_HIDDEN);
}

static void hide_keyboard(void)
{
    if (!sheet.keyboard)
        return;
    lv_obj_delete_async(sheet.keyboard);
    sheet.keyboard = NULL;
    render();
}

static void submit(void)
{
    if (!ready())
        return;
    net_ap_t ap = sheet.ap;
    wifi_join_cb_t on_join = sheet.on_join;
    bool joined = net_join(&ap, sheet.password);
    wifi_join_close();
    if (joined && on_join)
        on_join(ap.ssid);
}

static void on_key(uint32_t key, uint32_t mods, void *user)
{
    (void)mods;
    (void)user;
    if (key == SUI_KEY_HIDE || key == SUI_KEY_ESC)
        hide_keyboard();
    else if (key == SUI_KEY_ENTER)
        submit();
    else if (key == SUI_KEY_BACKSPACE && sheet.length > 0)
        sheet.password[--sheet.length] = '\0';
    else if (key >= ' ' && key < 0x7f && sheet.length + 1 < sizeof(sheet.password))
        sheet.password[sheet.length++] = (char)key;
    if (sheet.scrim)
        render();
}

static void show_keyboard(void)
{
    if (sheet.keyboard)
        return;
    sheet.keyboard = sui_keyboard_create(sheet.sheet, SUI_KEYBOARD_TEXT, on_key, NULL);
    render();
}

static void on_scrim(lv_event_t *e)
{
    if (lv_event_get_target(e) != sheet.scrim)
        return;
    if (sheet.keyboard)
        hide_keyboard();
    else
        wifi_join_close();
}

static void on_field(lv_event_t *e)
{
    (void)e;
    show_keyboard();
}

static void on_cancel(lv_event_t *e)
{
    (void)e;
    wifi_join_close();
}

static void on_submit(lv_event_t *e)
{
    (void)e;
    submit();
}

static void on_reveal(lv_event_t *e)
{
    (void)e;
    sheet.revealed = !sheet.revealed;
    sui_button_set_text(sheet.reveal, sheet.revealed ? "Hide" : "Show");
    render();
}

static void create_top_row(void)
{
    char title[TEXT], detail[TEXT];
    lv_obj_t *row = sui_box_create(sheet.sheet);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_style_pad_top(row, SHEET_PAD_TOP, 0);
    lv_obj_set_style_pad_hor(row, SUI_SPACE_XL, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_add_event_cb(sui_button_secondary_create(row, "Cancel"), on_cancel, LV_EVENT_CLICKED, NULL);

    lv_obj_t *titles = sui_box_create(row);
    lv_obj_set_flex_flow(titles, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(titles, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    snprintf(title, sizeof(title), "Join %s", sheet.ap.ssid);
    lv_obj_t *heading = lv_label_create(titles);
    lv_label_set_text(heading, title);
    lv_obj_set_style_text_font(heading, sui_font(SUI_TEXT_HEADING), 0);
    snprintf(detail, sizeof(detail), "%s · %s", net_band_label(sheet.ap.frequency), net_security_label(sheet.ap.security));
    lv_obj_t *sub = lv_label_create(titles);
    lv_label_set_text(sub, detail);
    lv_obj_set_style_text_font(sub, sui_font(SUI_TEXT_CAPTION), 0);
    lv_obj_set_style_text_color(sub, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);

    sheet.join = sui_button_create(row, "Join", SUI_COLOR_RAISED, SUI_COLOR_TEXT_MUTED);
    lv_obj_add_event_cb(sheet.join, on_submit, LV_EVENT_CLICKED, NULL);
}

static void create_field_row(void)
{
    lv_obj_t *row = sui_box_create(sheet.sheet);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_style_pad_all(row, SUI_SPACE_XL - 12, 0);
    lv_obj_set_style_pad_hor(row, SUI_SPACE_XL, 0);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    sheet.field = sui_box_create(row);
    lv_obj_add_flag(sheet.field, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(sheet.field, FIELD_H);
    lv_obj_set_flex_grow(sheet.field, 1);
    lv_obj_set_style_pad_hor(sheet.field, FIELD_PAD, 0);
    lv_obj_set_style_radius(sheet.field, FIELD_RADIUS, 0);
    lv_obj_set_style_bg_color(sheet.field, lv_color_hex(SUI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(sheet.field, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sheet.field, 2, 0);
    lv_obj_set_flex_flow(sheet.field, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sheet.field, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(sheet.field, on_field, LV_EVENT_CLICKED, NULL);

    sheet.text = lv_label_create(sheet.field);
    sheet.cursor = sui_box_create(sheet.field);
    lv_obj_set_size(sheet.cursor, CURSOR_W, CURSOR_H);
    lv_obj_set_style_bg_color(sheet.cursor, lv_color_hex(SUI_COLOR_ACCENT), 0);
    lv_obj_set_style_bg_opa(sheet.cursor, LV_OPA_COVER, 0);

    sheet.reveal = sui_button_secondary_create(row, "Show");
    lv_obj_set_height(sheet.reveal, FIELD_H);
    lv_obj_add_event_cb(sheet.reveal, on_reveal, LV_EVENT_CLICKED, NULL);
}

void wifi_join_open(const net_ap_t *ap, wifi_join_cb_t on_join)
{
    wifi_join_close();
    sheet.ap = *ap;
    sheet.on_join = on_join;

    sheet.scrim = sui_box_create(sui_layer_system());
    lv_obj_add_flag(sheet.scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(sheet.scrim, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(sheet.scrim, lv_color_hex(SCRIM_COLOR), 0);
    lv_obj_set_style_bg_opa(sheet.scrim, SCRIM_OPA, 0);
    lv_obj_add_event_cb(sheet.scrim, on_scrim, LV_EVENT_CLICKED, NULL);

    sheet.sheet = sui_box_create(sheet.scrim);
    lv_obj_add_flag(sheet.sheet, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_width(sheet.sheet, LV_PCT(100));
    lv_obj_set_style_pad_bottom(sheet.sheet, SHEET_RADIUS, 0);
    lv_obj_set_style_radius(sheet.sheet, SHEET_RADIUS, 0);
    lv_obj_set_style_clip_corner(sheet.sheet, true, 0);
    lv_obj_set_style_text_color(sheet.sheet, lv_color_hex(SUI_COLOR_TEXT), 0);
    lv_obj_set_style_bg_color(sheet.sheet, lv_color_hex(SUI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(sheet.sheet, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(sheet.sheet, lv_color_hex(SUI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(sheet.sheet, 1, 0);
    lv_obj_set_style_border_side(sheet.sheet, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_flex_flow(sheet.sheet, LV_FLEX_FLOW_COLUMN);
    lv_obj_align(sheet.sheet, LV_ALIGN_BOTTOM_MID, 0, SHEET_RADIUS);

    create_top_row();
    create_field_row();
    show_keyboard();
    sui_set_key_handler(on_key, NULL);
}

void wifi_join_close(void)
{
    if (!sheet.scrim)
        return;
    sui_set_key_handler(NULL, NULL);
    lv_obj_delete_async(sheet.scrim);
    wipe();
    memset(&sheet, 0, sizeof(sheet));
}
