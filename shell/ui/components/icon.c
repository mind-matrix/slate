#include "components/icon.h"

#include "theme/theme.h"

#define GRID 24
#define MAX_POINTS 10
#define MAX_PATHS 8

typedef struct {
    uint8_t count;
    uint8_t points[MAX_POINTS][2];
} polyline_t;

typedef struct {
    uint8_t circle_r;
    uint8_t dot_y;
    uint8_t path_count;
    polyline_t paths[MAX_PATHS];
} icon_shape_t;

static const icon_shape_t shapes[] = {
    [SUI_ICON_CLOCK] = { 10, 0, 1, { { 3, { { 12, 6 }, { 12, 12 }, { 16, 14 } } } } },
    [SUI_ICON_INFO] = { 10, 8, 1, { { 2, { { 12, 16 }, { 12, 11 } } } } },
    [SUI_ICON_TERMINAL] = { 0, 0, 2, { { 3, { { 4, 17 }, { 10, 12 }, { 4, 7 } } }, { 2, { { 12, 19 }, { 20, 19 } } } } },
    [SUI_ICON_PULSE] = { 0, 0, 1, { { 6, { { 3, 12 }, { 7, 12 }, { 10, 4 }, { 14, 20 }, { 17, 12 }, { 21, 12 } } } } },
    [SUI_ICON_MONITOR] = { 0, 0, 3,
                           { { 5, { { 2, 3 }, { 22, 3 }, { 22, 17 }, { 2, 17 }, { 2, 3 } } },
                             { 2, { { 8, 21 }, { 16, 21 } } },
                             { 2, { { 12, 17 }, { 12, 21 } } } } },
    [SUI_ICON_SUN] = { 4, 0, 8,
                       { { 2, { { 12, 2 }, { 12, 4 } } },
                         { 2, { { 12, 20 }, { 12, 22 } } },
                         { 2, { { 2, 12 }, { 4, 12 } } },
                         { 2, { { 20, 12 }, { 22, 12 } } },
                         { 2, { { 5, 5 }, { 6, 6 } } },
                         { 2, { { 18, 18 }, { 19, 19 } } },
                         { 2, { { 5, 19 }, { 6, 18 } } },
                         { 2, { { 18, 6 }, { 19, 5 } } } } },
    [SUI_ICON_LOCK] = { 0, 0, 2,
                        { { 5, { { 5, 11 }, { 19, 11 }, { 19, 21 }, { 5, 21 }, { 5, 11 } } },
                          { 6, { { 8, 11 }, { 8, 7 }, { 10, 4 }, { 14, 4 }, { 16, 7 }, { 16, 11 } } } } },
    [SUI_ICON_MIC] = { 0, 0, 3,
                       { { 9, { { 10, 2 }, { 14, 2 }, { 15, 4 }, { 15, 11 }, { 14, 13 }, { 10, 13 }, { 9, 11 }, { 9, 4 }, { 10, 2 } } },
                         { 5, { { 5, 10 }, { 7, 15 }, { 12, 17 }, { 17, 15 }, { 19, 10 } } },
                         { 2, { { 12, 17 }, { 12, 22 } } } } },
    [SUI_ICON_ALARM] = { 9, 0, 3,
                         { { 3, { { 12, 8 }, { 12, 12 }, { 15, 14 } } },
                           { 2, { { 2, 6 }, { 5, 3 } } },
                           { 2, { { 19, 3 }, { 22, 6 } } } } },
    [SUI_ICON_TIMER] = { 0, 0, 4,
                         { { 2, { { 6, 2 }, { 18, 2 } } },
                           { 2, { { 6, 22 }, { 18, 22 } } },
                           { 5, { { 8, 2 }, { 8, 7 }, { 12, 12 }, { 8, 17 }, { 8, 22 } } },
                           { 5, { { 16, 2 }, { 16, 7 }, { 12, 12 }, { 16, 17 }, { 16, 22 } } } } },
    [SUI_ICON_REMINDER] = { 0, 0, 2,
                            { { 5, { { 21, 12 }, { 21, 21 }, { 3, 21 }, { 3, 3 }, { 16, 3 } } },
                              { 3, { { 8, 11 }, { 12, 15 }, { 22, 4 } } } } },
};

static lv_point_precise_t to_point(const lv_area_t *box, const uint8_t p[2])
{
    int32_t size = lv_area_get_width(box);
    return (lv_point_precise_t){ box->x1 + p[0] * size / GRID, box->y1 + p[1] * size / GRID };
}

static void draw_circle(lv_layer_t *layer, const lv_area_t *box, lv_color_t color, int32_t radius, int32_t y, int32_t width)
{
    int32_t size = lv_area_get_width(box);
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.color = color;
    arc.width = width;
    arc.center = (lv_point_t){ box->x1 + size / 2, box->y1 + y * size / GRID };
    arc.radius = (uint16_t)(radius * size / GRID);
    arc.start_angle = 0;
    arc.end_angle = 360;
    lv_draw_arc(layer, &arc);
}

static void draw_icon(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    const icon_shape_t *shape = &shapes[(uintptr_t)lv_obj_get_user_data(obj)];
    lv_color_t color = lv_obj_get_style_line_color(obj, 0);
    lv_area_t box;
    lv_obj_get_coords(obj, &box);
    int32_t width = LV_MAX(2, lv_area_get_width(&box) / 12);

    if (shape->circle_r)
        draw_circle(layer, &box, color, shape->circle_r, GRID / 2, width);
    if (shape->dot_y)
        draw_circle(layer, &box, color, 1, shape->dot_y, width);

    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = color;
    line.width = width;
    line.round_start = 1;
    line.round_end = 1;
    for (uint8_t i = 0; i < shape->path_count; i++) {
        const polyline_t *path = &shape->paths[i];
        for (uint8_t j = 1; j < path->count; j++) {
            line.p1 = to_point(&box, path->points[j - 1]);
            line.p2 = to_point(&box, path->points[j]);
            lv_draw_line(layer, &line);
        }
    }
}

static const lv_font_t *symbol_font(int32_t size)
{
    if (size >= 44)
        return sui_font(SUI_TEXT_DISPLAY);
    if (size >= 30)
        return sui_font(SUI_TEXT_TITLE);
    if (size >= 22)
        return sui_font(SUI_TEXT_HEADING);
    return sui_font(SUI_TEXT_ROW);
}

lv_obj_t *sui_glyph_create(lv_obj_t *parent, sui_glyph_t glyph, int32_t size, uint32_t color)
{
    if (glyph.symbol) {
        lv_obj_t *label = lv_label_create(parent);
        lv_label_set_text(label, glyph.symbol);
        lv_obj_set_style_text_font(label, symbol_font(size), 0);
        lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
        return label;
    }

    lv_obj_t *icon = sui_box_create(parent);
    lv_obj_set_size(icon, size, size);
    lv_obj_set_user_data(icon, (void *)(uintptr_t)glyph.icon);
    lv_obj_set_style_line_color(icon, lv_color_hex(color), 0);
    lv_obj_add_event_cb(icon, draw_icon, LV_EVENT_DRAW_MAIN, NULL);
    return icon;
}
