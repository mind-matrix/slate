#include "clock_sheets.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "clock.h"
#include "clock_alerts.h"
#include "components/button.h"
#include "components/dialog.h"
#include "components/keyboard.h"
#include "components/roller.h"
#include "components/segmented.h"
#include "platform/platform.h"
#include "theme/theme.h"

#define SCRIM_COLOR 0x080C18
#define SCRIM_OPA 140
#define SHEET_RADIUS 24
#define SHEET_TOP 40
#define FIELD_H 64
#define FIELD_RADIUS 14
#define WHEEL_BOX_W 460
#define WHEEL_W 168
#define DAY_CHIP_H 56
#define DAY_CHOICE_H 80
#define PICK_DAYS 60
#define DAY_SECONDS 86400
#define TEXT 96
#define DAY_CHOICES 4

typedef enum {
    SHEET_ALARM,
    SHEET_REMINDER,
} sheet_kind_t;

typedef struct {
    sheet_kind_t kind;
    uint32_t id;
    clock_sheet_done_cb_t done;
    lv_obj_t *scrim;
    lv_obj_t *sheet;
    lv_obj_t *subtitle;
    lv_obj_t *save;
    lv_obj_t *body;
    lv_obj_t *field_row;
    lv_obj_t *field;
    lv_obj_t *field_text;
    lv_obj_t *keyboard;
    lv_obj_t *hour;
    lv_obj_t *minute;
    lv_obj_t *days[7];
    lv_obj_t *repeat_text;
    lv_obj_t *label_value;
    lv_obj_t *snooze;
    lv_obj_t *day_choices[DAY_CHOICES];
    lv_obj_t *pick_dialog;
    lv_obj_t *pick_roller;
    char text[CLOCK_LABEL_MAX];
    size_t length;
    uint8_t day_mask;
    int day_offset;
} sheet_t;

static sheet_t sheet;

static const char *const day_letters[7] = { "M", "T", "W", "T", "F", "S", "S" };
static const char *const day_full[7] = { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };
static const char *const snooze_labels[4] = { "5 min", "10 min", "15 min", "Off" };
static const uint8_t snooze_values[4] = { 5, 10, 15, 0 };

static void render(void);
static void on_key(uint32_t key, uint32_t mods, void *user);

static lv_obj_t *add_label(lv_obj_t *parent, const char *text, sui_text_t font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, sui_font(font), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

static lv_obj_t *add_section(lv_obj_t *parent, const char *title)
{
    lv_obj_t *section = sui_box_create(parent);
    lv_obj_set_size(section, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_row(section, 10, 0);
    lv_obj_set_flex_flow(section, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *label = add_label(section, title, SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);
    lv_obj_set_style_text_letter_space(label, 1, 0);
    lv_obj_set_style_pad_left(label, SUI_SPACE_S, 0);
    return section;
}

static void set_visible(lv_obj_t *obj, bool visible)
{
    if (!obj)
        return;
    if (visible)
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void finish(bool changed)
{
    clock_sheet_done_cb_t done = sheet.done;
    if (changed)
        clock_save();
    clock_sheet_close();
    if (changed && done)
        done();
}

void clock_sheet_close(void)
{
    if (sheet.pick_dialog)
        sui_dialog_close(sheet.pick_dialog);
    if (sheet.scrim)
        lv_obj_delete_async(sheet.scrim);
    memset(&sheet, 0, sizeof(sheet));
}

static void hide_keyboard(void)
{
    if (!sheet.keyboard)
        return;
    lv_obj_delete_async(sheet.keyboard);
    sheet.keyboard = NULL;
    render();
}

static void show_keyboard(void)
{
    if (sheet.keyboard)
        return;
    sheet.keyboard = sui_keyboard_create(sheet.sheet, SUI_KEYBOARD_TEXT, on_key, NULL);
    render();
}

static int64_t reminder_when(void)
{
    time_t now = time(NULL);
    struct tm at;
    localtime_r(&now, &at);
    at.tm_mday += sheet.day_offset;
    at.tm_hour = sui_roller_get(sheet.hour);
    at.tm_min = sui_roller_get(sheet.minute);
    at.tm_sec = 0;
    at.tm_isdst = -1;
    return (int64_t)mktime(&at);
}

static bool can_save(void)
{
    if (sheet.kind == SHEET_ALARM)
        return true;
    return sheet.length > 0 && reminder_when() > (int64_t)time(NULL);
}

static void render_alarm(void)
{
    clock_alarm_t probe = { .hour = (uint8_t)sui_roller_get(sheet.hour), .minute = (uint8_t)sui_roller_get(sheet.minute),
                            .days = sheet.day_mask };
    int64_t now = (int64_t)time(NULL);
    char until[CLOCK_FORMAT_MAX], text[TEXT];
    clock_format_until(clock_alarm_next(&probe, now) - now, until, sizeof(until));
    snprintf(text, sizeof(text), "Rings in %s", until);
    lv_label_set_text(sheet.subtitle, text);

    for (int i = 0; i < 7; i++) {
        if (sheet.day_mask >> i & 1)
            lv_obj_add_state(sheet.days[i], LV_STATE_CHECKED);
        else
            lv_obj_remove_state(sheet.days[i], LV_STATE_CHECKED);
    }
    if (sheet.day_mask == 0)
        snprintf(text, sizeof(text), "Rings once, then turns off.");
    else if (sheet.day_mask == CLOCK_EVERY_DAY)
        snprintf(text, sizeof(text), "Every day");
    else if (sheet.day_mask == CLOCK_WEEKDAYS)
        snprintf(text, sizeof(text), "Weekdays");
    else if (sheet.day_mask == CLOCK_WEEKEND)
        snprintf(text, sizeof(text), "Weekends");
    else {
        size_t used = (size_t)snprintf(text, sizeof(text), "Every ");
        for (int i = 0; i < 7; i++)
            if (sheet.day_mask >> i & 1 && used < sizeof(text))
                used += (size_t)snprintf(text + used, sizeof(text) - used, "%s%s", used > 6 ? ", " : "", day_full[i]);
    }
    lv_label_set_text(sheet.repeat_text, text);
    lv_label_set_text(sheet.label_value, sheet.length ? sheet.text : "None");
}

static void render_reminder(void)
{
    char text[TEXT];
    int64_t when = reminder_when();
    if (when <= (int64_t)time(NULL))
        snprintf(text, sizeof(text), "That time has already passed today");
    else {
        char day[32];
        time_t t = (time_t)when;
        struct tm local;
        localtime_r(&t, &local);
        strftime(day, sizeof(day), "%a %-d %b at %H:%M", &local);
        snprintf(text, sizeof(text), "%s", day);
    }
    lv_label_set_text(sheet.subtitle, text);
    for (int i = 0; i < DAY_CHOICES; i++) {
        bool picked = i == DAY_CHOICES - 1 ? sheet.day_offset >= DAY_CHOICES - 1 : sheet.day_offset == i;
        if (picked)
            lv_obj_add_state(sheet.day_choices[i], LV_STATE_CHECKED);
        else
            lv_obj_remove_state(sheet.day_choices[i], LV_STATE_CHECKED);
    }
}

static void render(void)
{
    if (!sheet.scrim)
        return;
    bool typing = sheet.keyboard != NULL;
    bool empty = sheet.length == 0;
    const char *placeholder = sheet.kind == SHEET_ALARM ? "Alarm label" : "What do you want to be reminded about?";
    lv_label_set_text(sheet.field_text, empty ? placeholder : sheet.text);
    lv_obj_set_style_text_color(sheet.field_text, lv_color_hex(empty ? SUI_COLOR_TEXT_MUTED : SUI_COLOR_TEXT), 0);
    lv_obj_set_style_border_color(sheet.field, lv_color_hex(typing ? SUI_COLOR_CAT_CLOCK : SUI_COLOR_BORDER), 0);
    set_visible(sheet.field_row, typing || sheet.kind == SHEET_REMINDER);
    set_visible(sheet.body, !typing);

    if (sheet.kind == SHEET_ALARM)
        render_alarm();
    else
        render_reminder();
    bool ready = can_save();
    lv_obj_set_style_bg_color(sheet.save, lv_color_hex(ready ? SUI_COLOR_CAT_CLOCK : SUI_COLOR_RAISED), 0);
    lv_obj_set_style_text_color(sheet.save, lv_color_hex(ready ? SUI_COLOR_ON_ACCENT : SUI_COLOR_TEXT_MUTED), 0);
}

static void on_key(uint32_t key, uint32_t mods, void *user)
{
    (void)mods;
    (void)user;
    if (key == SUI_KEY_HIDE || key == SUI_KEY_ESC || key == SUI_KEY_ENTER)
        hide_keyboard();
    else if (key == SUI_KEY_BACKSPACE && sheet.length > 0) {
        do
            sheet.length--;
        while (sheet.length > 0 && ((unsigned char)sheet.text[sheet.length] & 0xC0) == 0x80);
        sheet.text[sheet.length] = '\0';
    } else if (key >= ' ' && key < 0x7f && sheet.length + 1 < sizeof(sheet.text)) {
        sheet.text[sheet.length++] = (char)key;
        sheet.text[sheet.length] = '\0';
    }
    render();
}

static void on_scrim(lv_event_t *e)
{
    if (lv_event_get_target(e) != sheet.scrim)
        return;
    if (sheet.keyboard)
        hide_keyboard();
    else
        clock_sheet_close();
}

static void on_cancel(lv_event_t *e)
{
    (void)e;
    clock_sheet_close();
}

static void on_field(lv_event_t *e)
{
    (void)e;
    show_keyboard();
}

static void on_changed(lv_event_t *e)
{
    (void)e;
    render();
}

static void save_alarm(void)
{
    clock_alarm_t *a = sheet.id ? clock_find_alarm(sheet.id) : clock_add_alarm();
    if (!a)
        return;
    a->hour = (uint8_t)sui_roller_get(sheet.hour);
    a->minute = (uint8_t)sui_roller_get(sheet.minute);
    a->days = sheet.day_mask;
    a->snooze = snooze_values[sui_segmented_get(sheet.snooze) % 4];
    a->on = true;
    a->snoozed_until = 0;
    snprintf(a->label, sizeof(a->label), "%s", sheet.text);
    clock_alerts_handled(CLOCK_KEY_ALARM, a->id);
}

static void save_reminder(void)
{
    clock_reminder_t *r = sheet.id ? clock_find_reminder(sheet.id) : clock_add_reminder();
    if (!r)
        return;
    r->when = reminder_when();
    r->done = false;
    r->fired = false;
    snprintf(r->text, sizeof(r->text), "%s", sheet.text);
    clock_alerts_handled(CLOCK_KEY_REMINDER, r->id);
}

static void on_save(lv_event_t *e)
{
    (void)e;
    if (!can_save())
        return;
    if (sheet.kind == SHEET_ALARM)
        save_alarm();
    else
        save_reminder();
    finish(true);
}

static void on_delete(lv_event_t *e)
{
    (void)e;
    if (sheet.kind == SHEET_ALARM) {
        clock_alerts_handled(CLOCK_KEY_ALARM, sheet.id);
        clock_delete_alarm(sheet.id);
    } else {
        clock_alerts_handled(CLOCK_KEY_REMINDER, sheet.id);
        clock_delete_reminder(sheet.id);
    }
    finish(true);
}

static void on_day(lv_event_t *e)
{
    int day = (int)(uintptr_t)lv_event_get_user_data(e);
    sheet.day_mask ^= (uint8_t)(1u << day);
    render();
}

static void on_pick_done(lv_event_t *e)
{
    (void)e;
    sheet.day_offset = DAY_CHOICES - 1 + (int)lv_roller_get_selected(sheet.pick_roller);
    sui_dialog_close(sheet.pick_dialog);
    sheet.pick_dialog = NULL;
    render();
}

static void open_date_picker(void)
{
    static char options[PICK_DAYS * 16];
    size_t used = 0;
    time_t now = time(NULL);
    for (int i = 0; i < PICK_DAYS; i++) {
        struct tm day;
        time_t t = now + (time_t)(DAY_CHOICES - 1 + i) * DAY_SECONDS;
        localtime_r(&t, &day);
        if (i)
            options[used++] = '\n';
        used += strftime(options + used, sizeof(options) - used, "%a %-d %b", &day);
    }
    sheet.pick_dialog = sui_dialog_create("Pick a date", "");
    lv_obj_t *actions = sui_dialog_actions(sheet.pick_dialog);
    lv_obj_t *roller = sheet.pick_roller = lv_roller_create(sheet.pick_dialog);
    lv_roller_set_options(roller, options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(roller, 3);
    lv_obj_set_width(roller, LV_PCT(100));
    lv_obj_set_style_text_font(roller, sui_font(SUI_TEXT_HEADING), 0);
    lv_obj_set_style_text_color(roller, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(roller, 0, 0);
    lv_obj_set_style_text_color(roller, lv_color_hex(SUI_COLOR_TEXT), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(roller, lv_color_hex(SUI_COLOR_RAISED), LV_PART_SELECTED);
    lv_obj_set_style_radius(roller, SUI_RADIUS_ROW, LV_PART_SELECTED);
    if (sheet.day_offset >= DAY_CHOICES - 1)
        lv_roller_set_selected(roller, (uint32_t)(sheet.day_offset - (DAY_CHOICES - 1)), LV_ANIM_OFF);
    lv_obj_move_to_index(roller, lv_obj_get_index(actions));
    lv_obj_t *done = sui_button_create(actions, "Done", SUI_COLOR_CAT_CLOCK, SUI_COLOR_ON_ACCENT);
    lv_obj_add_event_cb(done, on_pick_done, LV_EVENT_CLICKED, NULL);
}

static void on_day_choice(lv_event_t *e)
{
    int choice = (int)(uintptr_t)lv_event_get_user_data(e);
    if (choice == DAY_CHOICES - 1)
        open_date_picker();
    else
        sheet.day_offset = choice;
    render();
}

static lv_obj_t *add_wheels(lv_obj_t *parent, int32_t box_w, int hour, int minute)
{
    lv_obj_t *box = sui_box_create(parent);
    lv_obj_set_size(box, box_w, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(box, SUI_SPACE_S, 0);
    lv_obj_set_style_radius(box, SUI_RADIUS_TILE, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(SUI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_column(box, SUI_SPACE_XS, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    int32_t wheel_w = box_w > 400 ? WHEEL_W : WHEEL_W - 32;
    sheet.hour = sui_roller_create(box, 23, wheel_w);
    add_label(box, ":", SUI_TEXT_DISPLAY, SUI_COLOR_TEXT_MUTED);
    sheet.minute = sui_roller_create(box, 59, wheel_w);
    sui_roller_set(sheet.hour, hour);
    sui_roller_set(sheet.minute, minute);
    lv_obj_add_event_cb(sheet.hour, on_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(sheet.minute, on_changed, LV_EVENT_VALUE_CHANGED, NULL);
    return box;
}

static lv_obj_t *add_delete(lv_obj_t *parent, const char *text)
{
    lv_obj_t *button = sui_button_create(parent, text, SUI_COLOR_SURFACE, SUI_COLOR_DANGER);
    lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(button, on_delete, LV_EVENT_CLICKED, NULL);
    return button;
}

static void open_frame(const char *title)
{
    sheet.scrim = sui_box_create(lv_layer_top());
    lv_obj_add_flag(sheet.scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(sheet.scrim, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(sheet.scrim, lv_color_hex(SCRIM_COLOR), 0);
    lv_obj_set_style_bg_opa(sheet.scrim, SCRIM_OPA, 0);
    lv_obj_add_event_cb(sheet.scrim, on_scrim, LV_EVENT_CLICKED, NULL);

    sheet.sheet = sui_box_create(sheet.scrim);
    lv_obj_add_flag(sheet.sheet, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(sheet.sheet, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(sheet.sheet, SUI_SCREEN_H - SHEET_TOP + SHEET_RADIUS, 0);
    lv_obj_align(sheet.sheet, LV_ALIGN_BOTTOM_MID, 0, SHEET_RADIUS);
    lv_obj_set_style_pad_bottom(sheet.sheet, SHEET_RADIUS, 0);
    lv_obj_set_style_radius(sheet.sheet, SHEET_RADIUS, 0);
    lv_obj_set_style_bg_color(sheet.sheet, lv_color_hex(SUI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(sheet.sheet, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(sheet.sheet, lv_color_hex(SUI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(sheet.sheet, 1, 0);
    lv_obj_set_style_border_side(sheet.sheet, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_text_color(sheet.sheet, lv_color_hex(SUI_COLOR_TEXT), 0);
    lv_obj_set_flex_flow(sheet.sheet, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *row = sui_box_create(sheet.sheet);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(row, 20, 0);
    lv_obj_set_style_pad_hor(row, SUI_SPACE_XL, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(sui_button_secondary_create(row, "Cancel"), on_cancel, LV_EVENT_CLICKED, NULL);
    lv_obj_t *titles = sui_box_create(row);
    lv_obj_set_flex_flow(titles, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(titles, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    add_label(titles, title, SUI_TEXT_HEADING, SUI_COLOR_TEXT);
    sheet.subtitle = add_label(titles, "", SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);
    sheet.save = sui_button_create(row, "Save", SUI_COLOR_CAT_CLOCK, SUI_COLOR_ON_ACCENT);
    lv_obj_add_event_cb(sheet.save, on_save, LV_EVENT_CLICKED, NULL);

    sheet.field_row = sui_box_create(sheet.sheet);
    lv_obj_set_size(sheet.field_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(sheet.field_row, 20, 0);
    lv_obj_set_style_pad_hor(sheet.field_row, SUI_SPACE_XL, 0);
    sheet.field = sui_box_create(sheet.field_row);
    lv_obj_add_flag(sheet.field, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(sheet.field, LV_PCT(100), FIELD_H);
    lv_obj_set_style_pad_hor(sheet.field, 20, 0);
    lv_obj_set_style_radius(sheet.field, FIELD_RADIUS, 0);
    lv_obj_set_style_bg_color(sheet.field, lv_color_hex(SUI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(sheet.field, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sheet.field, 2, 0);
    lv_obj_add_event_cb(sheet.field, on_field, LV_EVENT_CLICKED, NULL);
    sheet.field_text = add_label(sheet.field, "", SUI_TEXT_ROW, SUI_COLOR_TEXT);
    lv_obj_set_width(sheet.field_text, LV_PCT(100));
    lv_label_set_long_mode(sheet.field_text, LV_LABEL_LONG_DOT);
    lv_obj_align(sheet.field_text, LV_ALIGN_LEFT_MID, 0, 0);

    sheet.body = sui_box_create(sheet.sheet);
    lv_obj_set_size(sheet.body, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(sheet.body, SUI_SPACE_XL, 0);
    lv_obj_set_style_pad_top(sheet.body, SUI_SPACE_L, 0);
    lv_obj_set_style_pad_bottom(sheet.body, SUI_SPACE_S, 0);
    lv_obj_set_style_pad_column(sheet.body, SUI_SPACE_XL, 0);
    lv_obj_set_flex_flow(sheet.body, LV_FLEX_FLOW_ROW);
}

static void set_text(const char *text)
{
    snprintf(sheet.text, sizeof(sheet.text), "%s", text);
    sheet.length = strlen(sheet.text);
}

void clock_sheet_alarm_open(uint32_t id, clock_sheet_done_cb_t done)
{
    clock_sheet_close();
    const clock_alarm_t *a = id ? clock_find_alarm(id) : NULL;
    clock_alarm_t fresh = { .hour = 7, .days = CLOCK_WEEKDAYS, .snooze = CLOCK_DEFAULT_SNOOZE };
    if (!a)
        a = &fresh;
    sheet.kind = SHEET_ALARM;
    sheet.id = id && a != &fresh ? id : 0;
    sheet.done = done;
    sheet.day_mask = a->days;
    set_text(a->label);
    open_frame(sheet.id ? "Edit alarm" : "New alarm");

    add_wheels(sheet.body, WHEEL_BOX_W, a->hour, a->minute);
    lv_obj_t *side = sui_box_create(sheet.body);
    lv_obj_set_height(side, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(side, 1);
    lv_obj_set_style_pad_row(side, 20, 0);
    lv_obj_set_flex_flow(side, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *repeat = add_section(side, "REPEAT");
    lv_obj_t *chips = sui_box_create(repeat);
    lv_obj_set_size(chips, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(chips, SUI_SPACE_S, 0);
    lv_obj_set_flex_flow(chips, LV_FLEX_FLOW_ROW);
    for (int i = 0; i < 7; i++) {
        lv_obj_t *chip = sheet.days[i] = sui_button_secondary_create(chips, day_letters[i]);
        lv_obj_set_height(chip, DAY_CHIP_H);
        lv_obj_set_flex_grow(chip, 1);
        lv_obj_set_style_pad_hor(chip, 0, 0);
        lv_obj_set_style_radius(chip, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_text_font(chip, sui_font(SUI_TEXT_ROW), 0);
        lv_obj_set_style_bg_color(chip, lv_color_hex(SUI_COLOR_CAT_CLOCK), LV_STATE_CHECKED);
        lv_obj_set_style_text_color(chip, lv_color_hex(SUI_COLOR_ON_ACCENT), LV_STATE_CHECKED);
        lv_obj_add_event_cb(chip, on_day, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }
    sheet.repeat_text = add_label(repeat, "", SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);
    lv_obj_set_style_pad_left(sheet.repeat_text, SUI_SPACE_S, 0);

    lv_obj_t *card = sui_box_create(side);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_radius(card, SUI_RADIUS_ROW, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(SUI_COLOR_RAISED), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *label_row = sui_box_create(card);
    lv_obj_add_flag(label_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(label_row, LV_PCT(100), SUI_ROW_H);
    lv_obj_set_style_pad_hor(label_row, SUI_SPACE_L, 0);
    lv_obj_set_style_radius(label_row, SUI_RADIUS_ROW, 0);
    lv_obj_add_style(label_row, sui_style_pressed(), LV_STATE_PRESSED);
    lv_obj_set_flex_flow(label_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(label_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(label_row, on_field, LV_EVENT_CLICKED, NULL);
    add_label(label_row, "Label", SUI_TEXT_ROW, SUI_COLOR_TEXT);
    sheet.label_value = add_label(label_row, "", SUI_TEXT_BODY, SUI_COLOR_TEXT_MUTED);

    lv_obj_t *snooze_row = sui_box_create(card);
    lv_obj_set_size(snooze_row, LV_PCT(100), SUI_ROW_TALL_H);
    lv_obj_set_style_pad_left(snooze_row, SUI_SPACE_L, 0);
    lv_obj_set_style_pad_right(snooze_row, 12, 0);
    lv_obj_set_style_border_side(snooze_row, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(snooze_row, 1, 0);
    lv_obj_set_style_border_color(snooze_row, lv_color_hex(SUI_COLOR_BORDER), 0);
    lv_obj_set_flex_flow(snooze_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(snooze_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    add_label(snooze_row, "Snooze", SUI_TEXT_ROW, SUI_COLOR_TEXT);
    sheet.snooze = sui_segmented_create(snooze_row, snooze_labels, 4, SUI_COLOR_CAT_CLOCK);
    lv_obj_set_width(sheet.snooze, 400);
    size_t snooze_index = 1;
    for (size_t i = 0; i < 4; i++)
        if (snooze_values[i] == a->snooze)
            snooze_index = i;
    sui_segmented_set(sheet.snooze, snooze_index);

    if (sheet.id)
        add_delete(side, LV_SYMBOL_TRASH "  Delete alarm");
    render();
}

void clock_sheet_reminder_open(uint32_t id, clock_sheet_done_cb_t done)
{
    clock_sheet_close();
    const clock_reminder_t *r = id ? clock_find_reminder(id) : NULL;
    sheet.kind = SHEET_REMINDER;
    sheet.id = r ? id : 0;
    sheet.done = done;

    int64_t now = (int64_t)time(NULL);
    int64_t when = r ? r->when : now + 3600;
    time_t t = (time_t)when, n = (time_t)now;
    struct tm at, today;
    localtime_r(&t, &at);
    localtime_r(&n, &today);
    today.tm_hour = today.tm_min = today.tm_sec = 0;
    at.tm_isdst = today.tm_isdst = -1;
    struct tm midnight = at;
    midnight.tm_hour = midnight.tm_min = midnight.tm_sec = 0;
    sheet.day_offset = (int)((mktime(&midnight) - mktime(&today) + DAY_SECONDS / 2) / DAY_SECONDS);
    if (sheet.day_offset < 0)
        sheet.day_offset = 0;
    set_text(r ? r->text : "");
    open_frame(sheet.id ? "Edit reminder" : "New reminder");

    lv_obj_t *left = sui_box_create(sheet.body);
    lv_obj_set_height(left, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(left, 1);
    lv_obj_set_style_pad_row(left, 20, 0);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_t *day = add_section(left, "DAY");
    lv_obj_t *chips = sui_box_create(day);
    lv_obj_set_size(chips, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(chips, SUI_SPACE_S, 0);
    lv_obj_set_flex_flow(chips, LV_FLEX_FLOW_ROW);
    for (int i = 0; i < DAY_CHOICES; i++) {
        char text[48], date[24];
        time_t d = n + (time_t)i * DAY_SECONDS;
        struct tm local;
        localtime_r(&d, &local);
        strftime(date, sizeof(date), "%a %-d %b", &local);
        if (i == 0)
            snprintf(text, sizeof(text), "Today\n%s", date);
        else if (i == 1)
            snprintf(text, sizeof(text), "Tomorrow\n%s", date);
        else if (i == 2) {
            char name[16];
            strftime(name, sizeof(name), "%A", &local);
            snprintf(text, sizeof(text), "%s\n%s", name, date);
        } else
            snprintf(text, sizeof(text), "Pick date\n…");
        lv_obj_t *chip = sheet.day_choices[i] = sui_button_secondary_create(chips, text);
        lv_obj_set_height(chip, DAY_CHOICE_H);
        lv_obj_set_flex_grow(chip, 1);
        lv_obj_set_style_pad_hor(chip, 0, 0);
        lv_obj_set_style_radius(chip, FIELD_RADIUS, 0);
        lv_obj_set_style_text_align(chip, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_bg_color(chip, lv_color_hex(SUI_COLOR_CAT_CLOCK), LV_STATE_CHECKED);
        lv_obj_set_style_text_color(chip, lv_color_hex(SUI_COLOR_ON_ACCENT), LV_STATE_CHECKED);
        lv_obj_add_event_cb(chip, on_day_choice, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }
    lv_obj_t *hint = add_label(left,
                               "At the set time Slate wakes the screen, plays a short chime and adds the reminder to "
                               "Notifications.",
                               SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);
    lv_obj_set_width(hint, LV_PCT(100));
    lv_obj_set_style_pad_left(hint, SUI_SPACE_S, 0);
    if (sheet.id)
        add_delete(left, LV_SYMBOL_TRASH "  Delete reminder");

    lv_obj_t *right = add_section(sheet.body, "TIME");
    lv_obj_set_width(right, 380);
    add_wheels(right, 380, at.tm_hour, at.tm_min);

    if (!sheet.id)
        show_keyboard();
    render();
}
