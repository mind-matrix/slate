#include "components/status_bar.h"

#include <string.h>
#include <time.h>
#include "theme/theme.h"

#define CLOCK_PERIOD_MS 1000

static void update_clock(lv_timer_t *timer)
{
    lv_obj_t *label = lv_timer_get_user_data(timer);
    time_t now = time(NULL);
    struct tm local;
    char text[8];

    localtime_r(&now, &local);
    strftime(text, sizeof(text), "%H:%M", &local);
    if (strcmp(text, lv_label_get_text(label)) != 0)
        lv_label_set_text(label, text);
}

static lv_obj_t *add_icon(lv_obj_t *parent, const char *symbol)
{
    lv_obj_t *icon = lv_label_create(parent);
    lv_label_set_text(icon, symbol);
    lv_obj_set_style_text_color(icon, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    return icon;
}

lv_obj_t *sui_status_bar_create(lv_obj_t *parent)
{
    lv_obj_t *bar = sui_box_create(parent);
    lv_obj_set_size(bar, LV_PCT(100), SUI_STATUS_BAR_H);
    lv_obj_set_style_pad_hor(bar, SUI_SPACE_XL, 0);
    lv_obj_set_style_text_font(bar, sui_font(SUI_TEXT_BODY), 0);
    lv_obj_set_style_text_color(bar, lv_color_hex(SUI_COLOR_TEXT), 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *clock = lv_label_create(bar);
    lv_label_set_text(clock, "");
    lv_timer_t *timer = lv_timer_create(update_clock, CLOCK_PERIOD_MS, clock);
    update_clock(timer);

    lv_obj_t *icons = sui_box_create(bar);
    lv_obj_set_flex_flow(icons, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(icons, SUI_SPACE_L, 0);
    add_icon(icons, LV_SYMBOL_WIFI);
    add_icon(icons, LV_SYMBOL_BLUETOOTH);
    add_icon(icons, LV_SYMBOL_VOLUME_MAX);
    return bar;
}
