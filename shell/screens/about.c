#include "about.h"

#include <stdio.h>
#include "components/header.h"
#include "components/list.h"
#include "diagnostics.h"
#include "software_update.h"
#include "sysinfo.h"
#include "theme/theme.h"

#define COLUMN_W 596
#define GIB (1024.0 * 1024.0)

static lv_obj_t *add_column(lv_obj_t *parent, const char *section)
{
    lv_obj_t *column = sui_box_create(parent);
    lv_obj_set_width(column, COLUMN_W);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(column, SUI_SPACE_S + 2, 0);
    sui_section_label_create(column, section);
    return column;
}

static void add_value(lv_obj_t *list, const char *title, const char *value)
{
    sui_row_t row = { .title = title, .detail = value, .trailing = SUI_TRAILING_VALUE };
    sui_list_row_create(list, &row);
}

static void open_diagnostics(lv_event_t *e)
{
    (void)e;
    sui_nav_push(&diagnostics_screen, NULL);
}

static lv_obj_t *update_row;

static void open_update(lv_event_t *e)
{
    (void)e;
    sui_nav_push(&software_update_screen, NULL);
}

static void create(lv_obj_t *screen, void *args)
{
    (void)args;
    sysinfo_device_t device;
    sysinfo_sample_t sample;
    char hardware[2 * SYSINFO_TEXT];

    sysinfo_read_device(&device);
    sysinfo_sample(&sample);
    snprintf(hardware, sizeof(hardware), "%s · %.0f GB", device.model, sample.memory.total_kib / GIB);

    lv_obj_t *page = sui_page_create(screen, "About");
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(page, SUI_SPACE_L, 0);

    lv_obj_t *software = add_column(page, "SOFTWARE");
    lv_obj_t *software_list = sui_list_create(software, LV_PCT(100));
    add_value(software_list, "Build type", device.build_type);
    add_value(software_list, "Version", device.version);
    add_value(software_list, "Base", device.base);

    lv_obj_t *diagnostics_list = sui_list_create(software, LV_PCT(100));
    lv_obj_set_style_margin_top(diagnostics_list, SUI_SPACE_S, 0);
    sui_row_t update = { SUI_SYMBOL(LV_SYMBOL_DOWNLOAD), SUI_COLOR_CAT_SYSTEM, "Software update", "",
                         SUI_TRAILING_CHEVRON };
    update_row = sui_list_row_create(diagnostics_list, &update);
    lv_obj_add_event_cb(update_row, open_update, LV_EVENT_CLICKED, NULL);
    sui_row_t diagnostics = { SUI_VECTOR(SUI_ICON_PULSE), SUI_COLOR_CAT_SYSTEM, "System diagnostics",
                              "CPU, memory, network, storage", SUI_TRAILING_CHEVRON };
    lv_obj_t *row = sui_list_row_create(diagnostics_list, &diagnostics);
    lv_obj_add_event_cb(row, open_diagnostics, LV_EVENT_CLICKED, NULL);

    lv_obj_t *hw = add_column(page, "DEVICE");
    lv_obj_t *device_list = sui_list_create(hw, LV_PCT(100));
    add_value(device_list, "Device name", device.hostname);
    add_value(device_list, "Hardware", hardware);
    add_value(device_list, "Kernel", device.kernel);
    add_value(device_list, "IP address", sample.ip);
    add_value(device_list, "Bluetooth name", device.hostname);
}

static void on_show(lv_obj_t *screen)
{
    (void)screen;
    char text[SOFTWARE_UPDATE_SUMMARY];
    uint32_t color;
    software_update_summary(text, sizeof(text), &color);
    sui_list_row_set_detail(update_row, text);
    sui_list_row_set_detail_color(update_row, color);
}

const sui_screen_def_t about_screen = {
    .name = "about",
    .create = create,
    .on_show = on_show,
};
