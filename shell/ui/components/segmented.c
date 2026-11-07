#include "components/segmented.h"

#include "theme/theme.h"

#define SEGMENT_H 56

static void pick(lv_event_t *e)
{
    lv_obj_t *segment = lv_event_get_current_target(e);
    lv_obj_t *segmented = lv_obj_get_parent(segment);
    size_t index = (size_t)lv_obj_get_index(segment);
    if (index == sui_segmented_get(segmented))
        return;
    sui_segmented_set(segmented, index);
    lv_obj_send_event(segmented, LV_EVENT_VALUE_CHANGED, NULL);
}

lv_obj_t *sui_segmented_create(lv_obj_t *parent, const char *const *labels, size_t count, uint32_t color)
{
    lv_obj_t *segmented = sui_box_create(parent);
    lv_obj_set_width(segmented, LV_PCT(100));
    lv_obj_set_style_pad_all(segmented, SUI_SPACE_S, 0);
    lv_obj_set_style_pad_column(segmented, SUI_SPACE_S, 0);
    lv_obj_set_style_radius(segmented, SUI_RADIUS_ROW, 0);
    lv_obj_set_style_bg_color(segmented, lv_color_hex(SUI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(segmented, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(segmented, lv_color_hex(SUI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(segmented, 1, 0);
    lv_obj_set_flex_flow(segmented, LV_FLEX_FLOW_ROW);

    for (size_t i = 0; i < count; i++) {
        lv_obj_t *segment = sui_box_create(segmented);
        lv_obj_add_flag(segment, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_height(segment, SEGMENT_H);
        lv_obj_set_flex_grow(segment, 1);
        lv_obj_set_style_radius(segment, SUI_RADIUS_BUTTON, 0);
        lv_obj_set_style_text_font(segment, sui_font(SUI_TEXT_ROW), 0);
        lv_obj_set_style_text_color(segment, lv_color_hex(SUI_COLOR_TEXT), 0);
        lv_obj_set_style_bg_color(segment, lv_color_hex(color), LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(segment, LV_OPA_COVER, LV_STATE_CHECKED);
        lv_obj_set_style_text_color(segment, lv_color_hex(SUI_COLOR_ON_ACCENT), LV_STATE_CHECKED);
        lv_obj_add_style(segment, sui_style_pressed(), LV_STATE_PRESSED);
        lv_obj_add_event_cb(segment, pick, LV_EVENT_CLICKED, NULL);

        lv_obj_t *label = lv_label_create(segment);
        lv_label_set_text(label, labels[i]);
        lv_obj_center(label);
    }
    return segmented;
}

void sui_segmented_set(lv_obj_t *segmented, size_t index)
{
    uint32_t count = lv_obj_get_child_count(segmented);
    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t *segment = lv_obj_get_child(segmented, (int32_t)i);
        if (i == index)
            lv_obj_add_state(segment, LV_STATE_CHECKED);
        else
            lv_obj_remove_state(segment, LV_STATE_CHECKED);
    }
}

size_t sui_segmented_get(lv_obj_t *segmented)
{
    uint32_t count = lv_obj_get_child_count(segmented);
    for (uint32_t i = 0; i < count; i++)
        if (lv_obj_has_state(lv_obj_get_child(segmented, (int32_t)i), LV_STATE_CHECKED))
            return i;
    return count;
}
