#include "timezone_picker.h"

#include <stdio.h>
#include <string.h>
#include "components/header.h"
#include "components/list.h"
#include "datetime.h"
#include "theme/theme.h"
#include "timezone.h"

#define MAX_ZONES 512
#define MAX_REGIONS 16
#define REGION_W 300
#define REGION_H 60
#define REGION_GAP 4
#define PANE_PAD 8
#define TEXT 96

typedef struct {
    timezone_id_t *zones;
    size_t zone_count;
    char regions[MAX_REGIONS][TIMEZONE_ID_MAX];
    size_t region_count;
    size_t region;
    char current[TIMEZONE_ID_MAX];
    lv_obj_t *region_pane;
    lv_obj_t *cities;
    lv_obj_t *summary;
} picker_t;

static picker_t picker;

static void show_cities(void);

static void collect_regions(void)
{
    picker.region_count = 0;
    for (size_t i = 0; i < picker.zone_count && picker.region_count < MAX_REGIONS; i++) {
        char region[TIMEZONE_ID_MAX];
        timezone_region(picker.zones[i], region, sizeof(region));
        bool known = false;
        for (size_t j = 0; j < picker.region_count && !known; j++)
            known = strcmp(picker.regions[j], region) == 0;
        if (!known)
            snprintf(picker.regions[picker.region_count++], TIMEZONE_ID_MAX, "%s", region);
    }
}

static void style_region(lv_obj_t *button, bool selected)
{
    lv_obj_set_style_bg_color(button, lv_color_hex(SUI_COLOR_ACCENT_FILL), 0);
    lv_obj_set_style_bg_opa(button, selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(button, lv_color_hex(selected ? SUI_COLOR_WHITE : SUI_COLOR_TEXT), 0);
}

static void pick_region(lv_event_t *e)
{
    picker.region = (size_t)(uintptr_t)lv_event_get_user_data(e);
    for (size_t i = 0; i < picker.region_count; i++)
        style_region(lv_obj_get_child(picker.region_pane, (int32_t)i), i == picker.region);
    show_cities();
}

static void update_summary(void)
{
    char summary[TEXT], text[TEXT + 16];
    datetime_zone_summary(summary, sizeof(summary));
    snprintf(text, sizeof(text), "Current: %s", summary);
    lv_label_set_text(picker.summary, text);
}

static void update_checks(void)
{
    uint32_t count = lv_obj_get_child_count(picker.cities);
    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t *row = lv_obj_get_child(picker.cities, (int32_t)i);
        const char *id = lv_event_dsc_get_user_data(lv_obj_get_event_dsc(row, 0));
        sui_list_row_set_checked(row, strcmp(id, picker.current) == 0);
    }
}

static void pick_zone(lv_event_t *e)
{
    const char *id = lv_event_get_user_data(e);
    if (strcmp(id, picker.current) == 0 || !timezone_set(id))
        return;
    snprintf(picker.current, sizeof(picker.current), "%s", id);
    update_summary();
    update_checks();
}

static void show_cities(void)
{
    lv_obj_clean(picker.cities);
    lv_obj_scroll_to_y(picker.cities, 0, LV_ANIM_OFF);
    for (size_t i = 0; i < picker.zone_count; i++) {
        char region[TIMEZONE_ID_MAX], city[TIMEZONE_ID_MAX], offset[24];
        timezone_region(picker.zones[i], region, sizeof(region));
        if (strcmp(region, picker.regions[picker.region]) != 0)
            continue;
        timezone_city(picker.zones[i], city, sizeof(city));
        timezone_format_offset(timezone_offset(picker.zones[i]), offset, sizeof(offset));
        sui_row_t spec = { .title = city, .detail = offset, .trailing = SUI_TRAILING_RADIO };
        lv_obj_t *row = sui_list_row_create(picker.cities, &spec);
        sui_list_row_set_checked(row, strcmp(picker.zones[i], picker.current) == 0);
        lv_obj_add_event_cb(row, pick_zone, LV_EVENT_CLICKED, picker.zones[i]);
    }
}

static lv_obj_t *create_region_pane(lv_obj_t *parent)
{
    lv_obj_t *pane = sui_list_create(parent, REGION_W);
    lv_obj_set_height(pane, LV_PCT(100));
    lv_obj_set_style_pad_all(pane, PANE_PAD, 0);
    lv_obj_set_style_pad_row(pane, REGION_GAP, 0);
    lv_obj_add_flag(pane, LV_OBJ_FLAG_SCROLLABLE);

    for (size_t i = 0; i < picker.region_count; i++) {
        lv_obj_t *button = sui_box_create(pane);
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(button, LV_PCT(100), REGION_H);
        lv_obj_set_style_radius(button, SUI_RADIUS_BUTTON, 0);
        lv_obj_set_style_pad_hor(button, SUI_SPACE_L - 4, 0);
        lv_obj_set_style_text_font(button, sui_font(SUI_TEXT_ROW), 0);
        lv_obj_add_event_cb(button, pick_region, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        lv_obj_t *label = lv_label_create(button);
        lv_label_set_text(label, picker.regions[i]);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
        style_region(button, i == picker.region);
    }
    return pane;
}

static void release(lv_event_t *e)
{
    (void)e;
    lv_free(picker.zones);
    picker.zones = NULL;
}

static void create(lv_obj_t *screen, void *args)
{
    (void)args;
    char region[TIMEZONE_ID_MAX];

    picker.zones = lv_malloc(MAX_ZONES * sizeof(timezone_id_t));
    picker.zone_count = timezone_load_all(picker.zones, MAX_ZONES);
    timezone_current(picker.current, sizeof(picker.current));
    collect_regions();
    timezone_region(picker.current, region, sizeof(region));
    picker.region = 0;
    for (size_t i = 0; i < picker.region_count; i++)
        if (strcmp(picker.regions[i], region) == 0)
            picker.region = i;

    lv_obj_t *page = sui_page_create(screen, "Time zone");
    lv_obj_add_event_cb(page, release, LV_EVENT_DELETE, NULL);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(page, SUI_SPACE_L, 0);

    picker.region_pane = create_region_pane(page);
    picker.cities = sui_list_create(page, 0);
    lv_obj_set_flex_grow(picker.cities, 1);
    lv_obj_set_height(picker.cities, LV_PCT(100));
    lv_obj_add_flag(picker.cities, LV_OBJ_FLAG_SCROLLABLE);

    picker.summary = lv_label_create(screen);
    lv_obj_set_style_text_color(picker.summary, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    lv_obj_align(picker.summary, LV_ALIGN_TOP_RIGHT, -SUI_SPACE_XL, SUI_STATUS_BAR_H + SUI_HEADER_H / 2 - 10);
    update_summary();
    show_cities();
}

const sui_screen_def_t timezone_picker_screen = {
    .name = "timezone_picker",
    .create = create,
};
