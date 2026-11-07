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

typedef struct {
    lv_obj_t *slider;
    lv_obj_t *list;
    audio_device_t devices[AUDIO_MAX_DEVICES];
    size_t count;
    char signature[SIGNATURE_MAX];
} sound_column_t;

typedef struct {
    sound_column_t columns[2];
    lv_obj_t *mute;
    lv_timer_t *timer;
} sound_view_t;

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

static lv_obj_t *add_column(lv_obj_t *parent, audio_direction_t direction, const char *title, sui_glyph_t glyph)
{
    lv_obj_t *section = settings_section_create(parent, title, NULL);
    lv_obj_set_width(section, 0);
    lv_obj_set_flex_grow(section, 1);
    lv_obj_set_style_pad_row(section, 12, 0);

    sound_column_t *column = &view.columns[direction];
    column->slider = sui_slider_row_create(section, glyph, SUI_COLOR_ACCENT, 0, 100);
    lv_obj_add_event_cb(sui_slider_row_slider(column->slider), set_volume, LV_EVENT_RELEASED,
                        (void *)(uintptr_t)direction);
    column->list = sui_list_create(section, LV_PCT(100));
    return section;
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

    add_column(columns, AUDIO_OUTPUT, "OUTPUT", SUI_SYMBOL(LV_SYMBOL_VOLUME_MAX));
    lv_obj_t *mic = add_column(columns, AUDIO_INPUT, "MICROPHONE", SUI_VECTOR(SUI_ICON_MIC));
    view.mute = settings_switch_card_create(mic, "Mute microphone", NULL, SUI_COLOR_ACCENT);
    lv_obj_add_event_cb(view.mute, toggle_mute, LV_EVENT_VALUE_CHANGED, NULL);
}

static void on_show(lv_obj_t *screen)
{
    (void)screen;
    refresh();
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
