#include "notification_hub.h"

#include <stdio.h>
#include <time.h>
#include "components/button.h"
#include "components/icon.h"
#include "theme/theme.h"

#define HEADER_H 48
#define CARD_MIN_H 84
#define CARD_RADIUS 18
#define CARD_GAP 10
#define CHIP_SIZE 48
#define CHIP_RADIUS 14
#define CHIP_GLYPH 24
#define DISMISS_W 56
#define DISMISS_GLYPH 20
#define LETTER_SPACE 1
#define TITLE_MAX 48

typedef struct {
    sui_glyph_t glyph;
    uint32_t color;
} kind_t;

typedef struct {
    notification_open_cb_t open;
    lv_obj_t *title;
    lv_obj_t *clear;
    lv_obj_t *list;
    lv_obj_t *empty;
} hub_t;

typedef struct {
    notification_open_cb_t open;
    size_t index;
} pending_t;

static const kind_t kinds[NOTIFY_ICON_COUNT] = {
    [NOTIFY_ICON_ALARM] = { SUI_VECTOR(SUI_ICON_ALARM), SUI_COLOR_CAT_CLOCK },
    [NOTIFY_ICON_TIMER] = { SUI_VECTOR(SUI_ICON_TIMER), SUI_COLOR_CAT_CLOCK },
    [NOTIFY_ICON_REMINDER] = { SUI_VECTOR(SUI_ICON_REMINDER), SUI_COLOR_CAT_CLOCK },
    [NOTIFY_ICON_UPDATE] = { SUI_SYMBOL(LV_SYMBOL_DOWNLOAD), SUI_COLOR_CAT_SYSTEM },
    [NOTIFY_ICON_BLUETOOTH] = { SUI_SYMBOL(LV_SYMBOL_BLUETOOTH), SUI_COLOR_CAT_BLUETOOTH },
};

static pending_t pending;

static void run_pending(void *data)
{
    (void)data;
    const notify_list_t *list = notify_items();
    if (pending.index >= list->count)
        return;
    notify_owner_t owner = list->items[pending.index].owner;
    notify_dismiss(pending.index);
    if (pending.open)
        pending.open(owner);
}

static void schedule(lv_event_t *e, bool open)
{
    lv_obj_t *card = lv_obj_get_parent(lv_event_get_current_target(e));
    hub_t *hub = lv_event_get_user_data(e);
    pending = (pending_t){ .open = open ? hub->open : NULL, .index = (size_t)lv_obj_get_index(card) };
    lv_async_call(run_pending, NULL);
}

static void on_open(lv_event_t *e)
{
    schedule(e, true);
}

static void on_dismiss(lv_event_t *e)
{
    schedule(e, false);
}

static void on_clear(lv_event_t *e)
{
    (void)e;
    notify_clear_all();
}

static void on_delete(lv_event_t *e)
{
    lv_free(lv_event_get_user_data(e));
}

static lv_obj_t *add_label(lv_obj_t *parent, const char *text, sui_text_t font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, sui_font(font), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

static void add_card(hub_t *hub, const notify_t *item, int64_t now)
{
    const kind_t *kind = &kinds[item->icon < NOTIFY_ICON_COUNT ? item->icon : NOTIFY_ICON_ALARM];
    lv_obj_t *card = sui_box_create(hub->list);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(card, CARD_MIN_H, 0);
    lv_obj_set_style_radius(card, CARD_RADIUS, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(SUI_COLOR_RAISED), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *body = sui_box_create(card);
    lv_obj_add_flag(body, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_height(body, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_pad_ver(body, 14, 0);
    lv_obj_set_style_pad_left(body, 20, 0);
    lv_obj_set_style_pad_column(body, SUI_SPACE_M, 0);
    lv_obj_set_style_radius(body, CARD_RADIUS, 0);
    lv_obj_add_style(body, sui_style_pressed(), LV_STATE_PRESSED);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(body, on_open, LV_EVENT_CLICKED, hub);

    lv_obj_t *chip = sui_box_create(body);
    lv_obj_set_size(chip, CHIP_SIZE, CHIP_SIZE);
    lv_obj_set_style_radius(chip, CHIP_RADIUS, 0);
    lv_obj_set_style_bg_color(chip, lv_color_hex(kind->color), 0);
    lv_obj_set_style_bg_opa(chip, SUI_TINT_OPA, 0);
    lv_obj_center(sui_glyph_create(chip, kind->glyph, CHIP_GLYPH, kind->color));

    lv_obj_t *text = sui_box_create(body);
    lv_obj_set_height(text, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(text, 1);
    lv_obj_set_style_pad_row(text, SUI_SPACE_XS, 0);
    lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *top = sui_box_create(text);
    lv_obj_set_size(top, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(top, SUI_SPACE_M - 4, 0);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_t *title = add_label(top, item->title, SUI_TEXT_ROW, SUI_COLOR_TEXT);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(title, 1);
    char when[NOTIFY_WHEN_MAX], meta[NOTIFY_WHEN_MAX + 32];
    notify_format_when(item->time, now, when, sizeof(when));
    snprintf(meta, sizeof(meta), "%s · %s", notify_owner_label(item->owner), when);
    add_label(top, meta, SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);

    if (item->body[0]) {
        lv_obj_t *detail = add_label(text, item->body, SUI_TEXT_BODY, SUI_COLOR_TEXT_MUTED);
        lv_obj_set_width(detail, LV_PCT(100));
        lv_label_set_long_mode(detail, LV_LABEL_LONG_DOT);
    }

    lv_obj_t *dismiss = sui_box_create(card);
    lv_obj_add_flag(dismiss, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(dismiss, DISMISS_W, CARD_MIN_H);
    lv_obj_set_style_radius(dismiss, CARD_RADIUS, 0);
    lv_obj_add_style(dismiss, sui_style_pressed(), LV_STATE_PRESSED);
    lv_obj_add_event_cb(dismiss, on_dismiss, LV_EVENT_CLICKED, hub);
    lv_obj_center(sui_glyph_create(dismiss, SUI_SYMBOL(LV_SYMBOL_CLOSE), DISMISS_GLYPH, SUI_COLOR_TEXT_MUTED));
}

lv_obj_t *notification_hub_create(lv_obj_t *parent, notification_open_cb_t open)
{
    hub_t *hub = lv_malloc_zeroed(sizeof(hub_t));
    hub->open = open;

    lv_obj_t *root = sui_box_create(parent);
    lv_obj_set_user_data(root, hub);
    lv_obj_add_event_cb(root, on_delete, LV_EVENT_DELETE, hub);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(root, SUI_SPACE_XS, 0);

    lv_obj_t *header = sui_box_create(root);
    lv_obj_set_size(header, LV_PCT(100), HEADER_H);
    lv_obj_set_style_pad_left(header, SUI_SPACE_S, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    hub->title = add_label(header, "", SUI_TEXT_CAPTION, SUI_COLOR_TEXT_MUTED);
    lv_obj_set_style_text_letter_space(hub->title, LETTER_SPACE, 0);
    hub->clear = sui_button_create(header, "Clear all", SUI_COLOR_SURFACE, SUI_COLOR_ACCENT);
    lv_obj_set_style_bg_opa(hub->clear, LV_OPA_TRANSP, 0);
    lv_obj_set_height(hub->clear, HEADER_H);
    lv_obj_add_event_cb(hub->clear, on_clear, LV_EVENT_CLICKED, NULL);

    hub->list = sui_box_create(root);
    lv_obj_set_width(hub->list, LV_PCT(100));
    lv_obj_set_flex_grow(hub->list, 1);
    lv_obj_set_style_pad_row(hub->list, CARD_GAP, 0);
    lv_obj_set_flex_flow(hub->list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(hub->list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(hub->list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(hub->list, LV_SCROLLBAR_MODE_ACTIVE);

    hub->empty = add_label(root, "You're all caught up.", SUI_TEXT_BODY, SUI_COLOR_TEXT_MUTED);
    lv_obj_set_style_pad_left(hub->empty, SUI_SPACE_S, 0);
    lv_obj_set_style_pad_bottom(hub->empty, SUI_SPACE_M, 0);

    notification_hub_refresh(root);
    return root;
}

static void set_hidden(lv_obj_t *obj, bool hidden)
{
    if (hidden)
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

void notification_hub_refresh(lv_obj_t *root)
{
    hub_t *hub = lv_obj_get_user_data(root);
    const notify_list_t *items = notify_items();
    char title[TITLE_MAX];
    if (items->count)
        snprintf(title, sizeof(title), "NOTIFICATIONS · %zu", items->count);
    else
        snprintf(title, sizeof(title), "NOTIFICATIONS");
    lv_label_set_text(hub->title, title);
    set_hidden(hub->clear, !items->count);
    set_hidden(hub->list, !items->count);
    set_hidden(hub->empty, items->count);

    lv_obj_clean(hub->list);
    int64_t now = (int64_t)time(NULL);
    for (size_t i = 0; i < items->count; i++)
        add_card(hub, &items->items[i], now);
}
