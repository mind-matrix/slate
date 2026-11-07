#include "sound.h"

#include <stdio.h>
#include <string.h>
#include "audio.h"
#include "components/icon.h"
#include "components/list.h"
#include "components/slider_row.h"
#include "components/switch.h"
#include "settings_ui.h"
#include "theme/theme.h"

#define REFRESH_MS 5000
#define SIGNATURE_MAX 1024
#define COLUMN_GAP 24
#define LEVEL_TITLE_W 158
#define DUCK_VALUE_W 108
#define DUCK_TITLE_W 230
#define MIC_ROW_H 80

typedef struct {
    lv_obj_t *slider;
    lv_obj_t *list;
    audio_device_t devices[AUDIO_MAX_DEVICES];
    size_t count;
    char signature[SIGNATURE_MAX];
} sound_column_t;

typedef struct {
    sound_column_t columns[2];
    lv_obj_t *levels[AUDIO_ROLE_COUNT];
    lv_obj_t *duck;
    lv_obj_t *mute;
    lv_timer_t *timer;
} sound_view_t;

static const struct {
    const char *key;
    const char *title;
    const char *detail;
    sui_glyph_t glyph;
} levels[AUDIO_ROLE_COUNT] = {
    [AUDIO_ROLE_MEDIA] = { AUDIO_LEVEL_MEDIA, "Media", "Music, video, apps", SUI_SYMBOL(LV_SYMBOL_AUDIO) },
    [AUDIO_ROLE_ALARM] = { AUDIO_LEVEL_ALARM, "Alarms", "Alarms and timers", SUI_VECTOR(SUI_ICON_ALARM) },
    [AUDIO_ROLE_NOTIFICATION] = { AUDIO_LEVEL_NOTIFICATION, "Notifications", "Reminders, chimes",
                                  SUI_SYMBOL(LV_SYMBOL_BELL) },
};

static sound_view_t view;

void sound_summary(char *out, size_t len)
{
    audio_device_t devices[AUDIO_MAX_DEVICES];
    size_t count = audio_list(AUDIO_OUTPUT, devices, AUDIO_MAX_DEVICES);
    int percent;
    bool muted;
    const char *name = NULL;
    for (size_t i = 0; i < count; i++)
        if (devices[i].is_default)
            name = devices[i].name;
    if (!name || !audio_get_volume(AUDIO_OUTPUT, &percent, &muted))
        snprintf(out, len, "No output device");
    else if (muted)
        snprintf(out, len, "%s · muted", name);
    else
        snprintf(out, len, "%s · %d%%", name, percent);
}

static void pick_device(lv_event_t *e)
{
    sound_column_t *column = &view.columns[AUDIO_OUTPUT];
    size_t index = (size_t)(uintptr_t)lv_event_get_user_data(e);
    if (index >= column->count || column->devices[index].is_default)
        return;
    audio_set_default(column->devices[index].id);
    for (size_t i = 0; i < column->count; i++) {
        column->devices[i].is_default = i == index;
        sui_list_row_set_checked(lv_obj_get_child(column->list, (int32_t)i), i == index);
    }
}

static void show_devices(audio_direction_t direction)
{
    sound_column_t *column = &view.columns[direction];
    char signature[SIGNATURE_MAX] = "";
    size_t used = 0;
    column->count = audio_list(direction, column->devices, AUDIO_MAX_DEVICES);
    for (size_t i = 0; i < column->count; i++) {
        int n = snprintf(signature + used, sizeof(signature) - used, "%u|%s|%d;", column->devices[i].id,
                         column->devices[i].name, column->devices[i].is_default);
        if (n < 0 || (size_t)n >= sizeof(signature) - used)
            break;
        used += (size_t)n;
    }
    if (strcmp(signature, column->signature) == 0)
        return;
    snprintf(column->signature, sizeof(column->signature), "%s", signature);

    lv_obj_clean(column->list);
    for (size_t i = 0; i < column->count; i++) {
        const audio_device_t *device = &column->devices[i];
        bool selectable = direction == AUDIO_OUTPUT;
        sui_row_t spec = { .title = device->name, .detail = device->kind,
                           .trailing = selectable ? SUI_TRAILING_RADIO : SUI_TRAILING_NONE };
        lv_obj_t *row = sui_list_row_create(column->list, &spec);
        if (selectable) {
            sui_list_row_set_checked(row, device->is_default);
            lv_obj_add_event_cb(row, pick_device, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        }
    }
    if (!column->count) {
        sui_row_t spec = { .title = direction == AUDIO_OUTPUT ? "No output devices" : "No microphones",
                           .detail = "Connect a speaker or headset", .trailing = SUI_TRAILING_NONE };
        sui_list_row_create(column->list, &spec);
    }
}

static void refresh(void)
{
    for (int direction = AUDIO_OUTPUT; direction <= AUDIO_INPUT; direction++) {
        int percent;
        bool muted;
        bool available = audio_get_volume((audio_direction_t)direction, &percent, &muted);
        if (available)
            sui_slider_row_set_value(view.columns[direction].slider, percent);
        sui_slider_row_set_available(view.columns[direction].slider, available);
        if (direction == AUDIO_INPUT) {
            sui_switch_set(view.mute, available && muted);
            if (available)
                lv_obj_remove_state(view.mute, LV_STATE_DISABLED);
            else
                lv_obj_add_state(view.mute, LV_STATE_DISABLED);
        }
        show_devices((audio_direction_t)direction);
    }
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    refresh();
}

static void set_volume(lv_event_t *e)
{
    audio_direction_t direction = (audio_direction_t)(uintptr_t)lv_event_get_user_data(e);
    audio_set_volume(direction, (int)lv_slider_get_value(lv_event_get_target(e)));
    if (direction == AUDIO_INPUT)
        sui_switch_set(view.mute, false);
}

static void toggle_mute(lv_event_t *e)
{
    (void)e;
    audio_set_muted(AUDIO_INPUT, sui_switch_get(view.mute));
}

static lv_obj_t *add_level_row(lv_obj_t *card, sui_glyph_t glyph, uint32_t color, const char *title,
                               const char *detail, int32_t height, int32_t min)
{
    lv_obj_t *row = sui_slider_row_create(card, glyph, color, min, 100);
    lv_obj_set_height(row, height);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(row, lv_obj_get_index(row) ? 1 : 0, 0);
    sui_slider_row_set_title(row, title, detail, LEVEL_TITLE_W);
    return row;
}

static void show_duck_value(void)
{
    int value = (int)lv_slider_get_value(sui_slider_row_slider(view.duck));
    lv_obj_t *label = sui_slider_row_value(view.duck);
    if (value >= 100)
        lv_label_set_text(label, "No change");
    else if (value <= 0)
        lv_label_set_text(label, "Silent");
    else
        lv_label_set_text_fmt(label, "%d%%", value);
}

static void on_level(lv_event_t *e)
{
    const char *key = lv_event_get_user_data(e);
    int value = (int)lv_slider_get_value(lv_event_get_target(e));
    if (strcmp(key, AUDIO_LEVEL_DUCK) == 0) {
        value = (value + 5) / 10 * 10;
        lv_slider_set_value(lv_event_get_target(e), value, LV_ANIM_OFF);
        show_duck_value();
    }
    audio_set_level(key, value);
}

static void on_duck_move(lv_event_t *e)
{
    (void)e;
    show_duck_value();
}

static lv_obj_t *add_section(lv_obj_t *parent, const char *title)
{
    lv_obj_t *section = settings_section_create(parent, title, NULL);
    lv_obj_set_style_pad_row(section, 10, 0);
    return section;
}

static void create_levels(lv_obj_t *column)
{
    lv_obj_t *section = add_section(column, "VOLUME");
    lv_obj_t *card = settings_card_create(section);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_clip_corner(card, true, 0);

    lv_obj_t *master = view.columns[AUDIO_OUTPUT].slider =
        add_level_row(card, SUI_SYMBOL(LV_SYMBOL_VOLUME_MAX), SUI_COLOR_ACCENT, "Master", NULL, SUI_ROW_TALL_H, 0);
    lv_obj_add_event_cb(sui_slider_row_slider(master), set_volume, LV_EVENT_RELEASED, (void *)(uintptr_t)AUDIO_OUTPUT);

    for (size_t i = 0; i < AUDIO_ROLE_COUNT; i++) {
        lv_obj_t *row = view.levels[i] = add_level_row(card, levels[i].glyph, SUI_COLOR_TEXT_MUTED, levels[i].title,
                                                       levels[i].detail, SUI_ROW_H, 0);
        lv_obj_set_style_text_font(sui_slider_row_value(row), sui_font(SUI_TEXT_BODY), 0);
        lv_obj_add_event_cb(sui_slider_row_slider(row), on_level, LV_EVENT_RELEASED, (void *)levels[i].key);
    }
    lv_obj_t *note = lv_label_create(section);
    lv_label_set_text(note, "Each type is a share of Master. The drawer shows Master only.");
    lv_obj_set_style_text_font(note, sui_font(SUI_TEXT_CAPTION), 0);
    lv_obj_set_style_text_color(note, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_pad_left(note, SUI_SPACE_S, 0);

    section = add_section(column, "DURING ALERTS");
    view.duck = add_level_row(section, SUI_SYMBOL(LV_SYMBOL_AUDIO), SUI_COLOR_ACCENT, "Lower media to",
                              "while an alarm or chime plays", SUI_ROW_TALL_H, 0);
    lv_obj_set_width(lv_obj_get_child(view.duck, 1), DUCK_TITLE_W);
    lv_obj_set_style_bg_opa(view.duck, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(view.duck, SUI_RADIUS_ROW, 0);
    lv_obj_set_style_border_side(view.duck, LV_BORDER_SIDE_FULL, 0);
    lv_obj_set_style_border_width(view.duck, 1, 0);
    lv_obj_set_width(sui_slider_row_value(view.duck), DUCK_VALUE_W);
    lv_obj_add_event_cb(sui_slider_row_slider(view.duck), on_duck_move, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(sui_slider_row_slider(view.duck), on_level, LV_EVENT_RELEASED, (void *)AUDIO_LEVEL_DUCK);
}

static void refresh_levels(void)
{
    for (size_t i = 0; i < AUDIO_ROLE_COUNT; i++)
        sui_slider_row_set_value(view.levels[i], audio_level(levels[i].key));
    sui_slider_row_set_value(view.duck, audio_level(AUDIO_LEVEL_DUCK));
    show_duck_value();
}

static lv_obj_t *add_column(lv_obj_t *parent)
{
    lv_obj_t *column = sui_box_create(parent);
    lv_obj_set_size(column, 0, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(column, 1);
    lv_obj_set_style_pad_row(column, 18, 0);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    return column;
}

static void create(lv_obj_t *screen, void *args)
{
    (void)args;
    memset(&view, 0, sizeof(view));
    lv_obj_t *page = settings_page_create(screen, "Sound");

    lv_obj_t *columns = sui_box_create(page);
    lv_obj_set_width(columns, LV_PCT(100));
    lv_obj_set_style_pad_column(columns, COLUMN_GAP, 0);
    lv_obj_set_flex_flow(columns, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(columns, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    create_levels(add_column(columns));

    lv_obj_t *right = add_column(columns);
    lv_obj_t *output = add_section(right, "OUTPUT");
    view.columns[AUDIO_OUTPUT].list = sui_list_create(output, LV_PCT(100));

    lv_obj_t *mic = add_section(right, "MICROPHONE");
    lv_obj_t *card = settings_card_create(mic);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_clip_corner(card, true, 0);
    sound_column_t *input = &view.columns[AUDIO_INPUT];
    input->slider = add_level_row(card, SUI_VECTOR(SUI_ICON_MIC), SUI_COLOR_ACCENT, "Level", NULL, MIC_ROW_H, 0);
    lv_obj_add_event_cb(sui_slider_row_slider(input->slider), set_volume, LV_EVENT_RELEASED,
                        (void *)(uintptr_t)AUDIO_INPUT);
    input->list = sui_list_create(card, LV_PCT(100));
    lv_obj_set_style_border_width(input->list, 0, 0);
    lv_obj_set_style_border_side(input->list, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(input->list, 1, 0);
    lv_obj_set_style_radius(input->list, 0, 0);
    view.mute = settings_switch_card_create(mic, "Mute microphone", NULL, SUI_COLOR_ACCENT);
    lv_obj_add_event_cb(view.mute, toggle_mute, LV_EVENT_VALUE_CHANGED, NULL);
}

static void on_show(lv_obj_t *screen)
{
    (void)screen;
    refresh();
    refresh_levels();
    view.timer = lv_timer_create(tick, REFRESH_MS, NULL);
}

static void on_hide(lv_obj_t *screen)
{
    (void)screen;
    lv_timer_delete(view.timer);
    view.timer = NULL;
}

const sui_screen_def_t sound_screen = {
    .name = "sound",
    .create = create,
    .on_show = on_show,
    .on_hide = on_hide,
};
