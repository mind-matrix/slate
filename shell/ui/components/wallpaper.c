#include "components/wallpaper.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "theme/theme.h"

#define SEAM_WIDTH 2
#define MARK_OFFSET_X -40
#define MARK_OFFSET_Y -16
#define MARK_LETTER_SPACE 6
#define DESIGN_W 1280
#define DESIGN_H 720
#define STOP_MAX LV_GRADIENT_MAX_STOPS
#define STOP_SPAN 255

typedef struct {
    int32_t left_y;
    int32_t right_y;
    uint32_t fill;
    uint32_t seam;
    lv_opa_t seam_opa;
} stratum_t;

typedef struct {
    const char *key;
    const char *name;
    uint8_t stop_count;
    uint32_t stops[STOP_MAX];
} wallpaper_t;

static const wallpaper_t wallpapers[SUI_WALLPAPER_COUNT] = {
    { "slate", "Slate", 0, { 0 } },
    { "dusk", "Dusk", 3, { 0x1B2540, 0x4A3570, 0xC9729A } },
    { "ocean", "Ocean", 3, { 0x0B1A33, 0x12446B, 0x3FA7A0 } },
    { "ember", "Ember", 3, { 0x121A2E, 0xB8452F, 0xFFC65C } },
    { "forest", "Forest", 3, { 0x0F2A22, 0x1E5B45, 0x7FBF8A } },
    { "aurora", "Aurora", 4, { 0x0B1022, 0x1E5A6B, 0x6BD3B0, 0xA99BFF } },
    { "sand", "Sand", 3, { 0x3A2E28, 0x8A6A4E, 0xE0B98A } },
    { "graphite", "Graphite", 2, { 0x2A303D, 0x14171E } },
};

static const stratum_t strata[] = {
    { 470, 250, 0x141D34, SUI_COLOR_CAT_SYSTEM, 56 },
    { 540, 330, 0x17213B, SUI_COLOR_CAT_BLUETOOTH, 56 },
    { 600, 410, 0x1B2644, SUI_COLOR_ACCENT, 115 },
    { 650, 490, 0x1F2B4D, SUI_COLOR_CAT_SOUND, 46 },
    { 700, 580, 0x233157, SUI_COLOR_CAT_DISPLAY, 46 },
};

static lv_grad_dsc_t gradients[SUI_WALLPAPER_COUNT];

static int valid_id(int id)
{
    return id >= 0 && id < SUI_WALLPAPER_COUNT ? id : SUI_WALLPAPER_DEFAULT;
}

const char *sui_wallpaper_name(int id)
{
    return wallpapers[valid_id(id)].name;
}

const char *sui_wallpaper_key(int id)
{
    return wallpapers[valid_id(id)].key;
}

int sui_wallpaper_find(const char *key)
{
    for (int i = 0; key && i < SUI_WALLPAPER_COUNT; i++)
        if (strcmp(wallpapers[i].key, key) == 0)
            return i;
    return SUI_WALLPAPER_DEFAULT;
}

static const lv_grad_dsc_t *gradient_for(int id)
{
    const wallpaper_t *paper = &wallpapers[id];
    lv_grad_dsc_t *grad = &gradients[id];
    if (grad->stops_count == 0) {
        grad->dir = LV_GRAD_DIR_VER;
        grad->stops_count = paper->stop_count;
        for (uint8_t i = 0; i < paper->stop_count; i++) {
            grad->stops[i].color = lv_color_hex(paper->stops[i]);
            grad->stops[i].opa = LV_OPA_COVER;
            grad->stops[i].frac = (uint8_t)(i * STOP_SPAN / (paper->stop_count - 1));
        }
    }
    return grad;
}

static int32_t scale(int32_t value, int32_t size, int32_t design)
{
    return value * size / design;
}

static void draw_stratum(lv_layer_t *layer, const lv_area_t *box, const stratum_t *s)
{
    int32_t width = lv_area_get_width(box);
    int32_t height = lv_area_get_height(box);
    int32_t left = box->x1;
    int32_t right = box->x2 + 1;
    int32_t left_y = box->y1 + scale(s->left_y, height, DESIGN_H);
    int32_t right_y = box->y1 + scale(s->right_y, height, DESIGN_H);
    int32_t seam_width = scale(SEAM_WIDTH, width, DESIGN_W);

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
    seam.width = seam_width > 0 ? seam_width : 1;
    seam.p1 = (lv_point_precise_t){ left, left_y };
    seam.p2 = (lv_point_precise_t){ right, right_y };
    lv_draw_line(layer, &seam);
}

static void draw_strata(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    if ((intptr_t)lv_obj_get_user_data(obj) != SUI_WALLPAPER_DEFAULT)
        return;
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t box;

    lv_obj_get_coords(obj, &box);
    for (size_t i = 0; i < sizeof(strata) / sizeof(strata[0]); i++)
        draw_stratum(layer, &box, &strata[i]);
}

void sui_wallpaper_set(lv_obj_t *wallpaper, int id)
{
    id = valid_id(id);
    lv_obj_set_user_data(wallpaper, (void *)(intptr_t)id);
    lv_obj_set_style_bg_opa(wallpaper, LV_OPA_COVER, 0);
    if (id == SUI_WALLPAPER_DEFAULT) {
        lv_obj_set_style_bg_grad_dir(wallpaper, LV_GRAD_DIR_NONE, 0);
        lv_obj_set_style_bg_color(wallpaper, lv_color_hex(SUI_COLOR_BG), 0);
    } else {
        lv_obj_set_style_bg_grad(wallpaper, gradient_for(id), 0);
    }
    lv_obj_invalidate(wallpaper);
}

lv_obj_t *sui_wallpaper_swatch_create(lv_obj_t *parent, int id, int32_t width, int32_t height)
{
    lv_obj_t *wallpaper = sui_box_create(parent);
    lv_obj_set_size(wallpaper, width, height);
    lv_obj_add_event_cb(wallpaper, draw_strata, LV_EVENT_DRAW_MAIN, NULL);
    sui_wallpaper_set(wallpaper, id);
    return wallpaper;
}

lv_obj_t *sui_wallpaper_create(lv_obj_t *parent, int id)
{
    lv_obj_t *wallpaper = sui_wallpaper_swatch_create(parent, id, LV_PCT(100), LV_PCT(100));

    lv_obj_t *mark = lv_label_create(wallpaper);
    lv_label_set_text(mark, "SLATE");
    lv_obj_set_style_text_font(mark, sui_font(SUI_TEXT_CAPTION), 0);
    lv_obj_set_style_text_color(mark, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_text_letter_space(mark, MARK_LETTER_SPACE, 0);
    lv_obj_set_style_text_opa(mark, LV_OPA_50, 0);
    lv_obj_align(mark, LV_ALIGN_BOTTOM_RIGHT, MARK_OFFSET_X, MARK_OFFSET_Y);
    return wallpaper;
}
