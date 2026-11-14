#include "wallpaper_screen.h"

#include "components/wallpaper.h"
#include "prefs.h"
#include "settings_ui.h"
#include "theme/theme.h"

#define SETTING "wallpaper"
#define KEY_MAX 32
#define LEFT_W 400
#define PREVIEW_H 225
#define COLUMN_GAP 32
#define GRID_COLUMNS 4
#define GRID_GAP 12
#define THUMB_H 112
#define THUMB_RADIUS 14
#define PREVIEW_RADIUS 16
#define RING_W 3
#define NAME_PAD 8
#define NAME_INSET 8
#define CAPTION_TEXT "Tap a wallpaper to use it. It shows behind the desktop."
#define SCRIM_OPA 115

static struct {
    lv_obj_t *preview;
    lv_obj_t *name;
    lv_obj_t *thumbs[SUI_WALLPAPER_COUNT];
} view;

static int current = -1;

int wallpaper_current(void)
{
    if (current < 0) {
        char key[KEY_MAX] = "";
        prefs_get_text(SETTING, key, sizeof(key));
        current = sui_wallpaper_find(key);
    }
    return current;
}

void wallpaper_select(int id)
{
    current = id;
    prefs_set_text(SETTING, sui_wallpaper_key(id));
}

static lv_obj_t *add_label(lv_obj_t *parent, const char *text, sui_text_t font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, sui_font(font), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

static lv_obj_t *frame_create(lv_obj_t *parent, int32_t width, int32_t height, int32_t radius)
{
    lv_obj_t *frame = sui_box_create(parent);
    lv_obj_set_size(frame, width, height);
    lv_obj_set_style_radius(frame, radius, 0);
    lv_obj_set_style_clip_corner(frame, true, 0);
    lv_obj_set_style_border_width(frame, 1, 0);
    lv_obj_set_style_border_color(frame, lv_color_hex(SUI_COLOR_BORDER), 0);
    return frame;
}

static void refresh(void)
{
    int id = wallpaper_current();
    sui_wallpaper_set(lv_obj_get_child(view.preview, 0), id);
    lv_label_set_text(view.name, sui_wallpaper_name(id));
    for (int i = 0; i < SUI_WALLPAPER_COUNT; i++) {
        bool selected = i == id;
        lv_obj_set_style_border_width(view.thumbs[i], selected ? RING_W : 1, 0);
        lv_obj_set_style_border_color(view.thumbs[i], lv_color_hex(selected ? SUI_COLOR_ACCENT : SUI_COLOR_BORDER), 0);
    }
}

static void pick(lv_event_t *e)
{
    wallpaper_select((int)(intptr_t)lv_event_get_user_data(e));
    refresh();
}

static void thumb_create(lv_obj_t *parent, int id, int32_t width)
{
    lv_obj_t *frame = frame_create(parent, width, THUMB_H, THUMB_RADIUS);
    lv_obj_add_flag(frame, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_style(frame, sui_style_pressed(), LV_STATE_PRESSED);
    lv_obj_add_event_cb(frame, pick, LV_EVENT_CLICKED, (void *)(intptr_t)id);
    lv_obj_t *swatch = sui_wallpaper_swatch_create(frame, id, LV_PCT(100), LV_PCT(100));
    lv_obj_remove_flag(swatch, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *chip = sui_box_create(frame);
    lv_obj_set_style_pad_hor(chip, NAME_PAD, 0);
    lv_obj_set_style_radius(chip, THUMB_RADIUS / 2, 0);
    lv_obj_set_style_bg_color(chip, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(chip, SCRIM_OPA, 0);
    lv_obj_align(chip, LV_ALIGN_BOTTOM_LEFT, NAME_INSET, -NAME_INSET);
    add_label(chip, sui_wallpaper_name(id), SUI_TEXT_CAPTION, SUI_COLOR_WHITE);
    view.thumbs[id] = frame;
}

static void create(lv_obj_t *screen, void *args)
{
    (void)args;
    lv_obj_t *page = settings_page_create(screen, "Wallpaper");
    lv_obj_t *columns = sui_box_create(page);
    lv_obj_set_width(columns, LV_PCT(100));
    lv_obj_set_flex_flow(columns, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(columns, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(columns, COLUMN_GAP, 0);

    lv_obj_t *left = settings_section_create(columns, "CURRENT", NULL);
    lv_obj_set_width(left, LEFT_W);
    view.preview = frame_create(left, LEFT_W, PREVIEW_H, PREVIEW_RADIUS);
    sui_wallpaper_create(view.preview, SUI_WALLPAPER_DEFAULT);
    view.name = add_label(left, "", SUI_TEXT_ROW, SUI_COLOR_TEXT);
    lv_obj_t *caption = add_label(left, CAPTION_TEXT, SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);
    lv_obj_set_width(caption, LEFT_W);

    lv_obj_t *right = settings_section_create(columns, "BUILT IN", NULL);
    int32_t right_w = lv_display_get_horizontal_resolution(NULL) - 2 * SUI_SPACE_XL - LEFT_W - COLUMN_GAP;
    lv_obj_set_width(right, right_w);
    lv_obj_t *grid = sui_box_create(right);
    lv_obj_set_width(grid, LV_PCT(100));
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(grid, GRID_GAP, 0);
    lv_obj_set_style_pad_row(grid, GRID_GAP, 0);
    int32_t width = (right_w - GRID_GAP * (GRID_COLUMNS - 1)) / GRID_COLUMNS;
    for (int i = 0; i < SUI_WALLPAPER_COUNT; i++)
        thumb_create(grid, i, width);
    refresh();
}

const sui_screen_def_t wallpaper_screen = {
    .name = "wallpaper",
    .create = create,
};
