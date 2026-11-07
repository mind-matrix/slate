#include "components/wallpaper.h"

#include "theme/theme.h"

#define SEAM_WIDTH 2
#define MARK_OFFSET_X -40
#define MARK_OFFSET_Y -16
#define MARK_LETTER_SPACE 6

typedef struct {
    int32_t left_y;
    int32_t right_y;
    uint32_t fill;
    uint32_t seam;
    lv_opa_t seam_opa;
} stratum_t;

static const stratum_t strata[] = {
    { 470, 250, 0x141D34, SUI_COLOR_CAT_SYSTEM, 56 },
    { 540, 330, 0x17213B, SUI_COLOR_CAT_BLUETOOTH, 56 },
    { 600, 410, 0x1B2644, SUI_COLOR_ACCENT, 115 },
    { 650, 490, 0x1F2B4D, SUI_COLOR_CAT_SOUND, 46 },
    { 700, 580, 0x233157, SUI_COLOR_CAT_DISPLAY, 46 },
};

static void draw_stratum(lv_layer_t *layer, const lv_area_t *box, const stratum_t *s)
{
    int32_t left = box->x1;
    int32_t right = box->x2 + 1;
    int32_t left_y = box->y1 + s->left_y;
    int32_t right_y = box->y1 + s->right_y;

    lv_draw_rect_dsc_t base;
    lv_draw_rect_dsc_init(&base);
    base.bg_color = lv_color_hex(s->fill);
    lv_area_t below = { left, left_y, box->x2, box->y2 };
    lv_draw_rect(layer, &base, &below);

    lv_draw_triangle_dsc_t wedge;
    lv_draw_triangle_dsc_init(&wedge);
    wedge.bg_color = lv_color_hex(s->fill);
    wedge.p[0] = (lv_point_precise_t){ left, left_y };
    wedge.p[1] = (lv_point_precise_t){ right, right_y };
    wedge.p[2] = (lv_point_precise_t){ right, left_y };
    lv_draw_triangle(layer, &wedge);

    lv_draw_line_dsc_t seam;
    lv_draw_line_dsc_init(&seam);
    seam.color = lv_color_hex(s->seam);
    seam.opa = s->seam_opa;
    seam.width = SEAM_WIDTH;
    seam.p1 = (lv_point_precise_t){ left, left_y };
    seam.p2 = (lv_point_precise_t){ right, right_y };
    lv_draw_line(layer, &seam);
}

static void draw_strata(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t box;

    lv_obj_get_coords(obj, &box);
    for (size_t i = 0; i < sizeof(strata) / sizeof(strata[0]); i++)
        draw_stratum(layer, &box, &strata[i]);
}

lv_obj_t *sui_wallpaper_create(lv_obj_t *parent)
{
    lv_obj_t *wallpaper = sui_box_create(parent);
    lv_obj_set_size(wallpaper, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(wallpaper, lv_color_hex(SUI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(wallpaper, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(wallpaper, draw_strata, LV_EVENT_DRAW_MAIN, NULL);

    lv_obj_t *mark = lv_label_create(wallpaper);
    lv_label_set_text(mark, "SLATE");
    lv_obj_set_style_text_font(mark, sui_font(SUI_TEXT_CAPTION), 0);
    lv_obj_set_style_text_color(mark, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_text_letter_space(mark, MARK_LETTER_SPACE, 0);
    lv_obj_set_style_text_opa(mark, LV_OPA_50, 0);
    lv_obj_align(mark, LV_ALIGN_BOTTOM_RIGHT, MARK_OFFSET_X, MARK_OFFSET_Y);
    return wallpaper;
}
