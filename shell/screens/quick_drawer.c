#include "quick_drawer.h"

#include <time.h>
#include "audio.h"
#include "backlight.h"
#include "bluetooth.h"
#include "bt.h"
#include "clock_app.h"
#include "components/icon.h"
#include "components/quick_tile.h"
#include "components/slider_row.h"
#include "net.h"
#include "nav/nav.h"
#include "notification_hub.h"
#include "notify.h"
#include "platform/platform.h"
#include "power_screen.h"
#include "settings.h"
#include "software_update.h"
#include "theme/theme.h"
#include "wifi.h"

#define PANEL_RADIUS 28
#define PANEL_PAD_TOP 20
#define PANEL_PAD_BOTTOM 0
#define PANEL_GAP 20
#define CONTROLS_W 720
#define CONTROL_GAP 12
#define SLIDER_ROW_H 72
#define ACTION_SIZE 56
#define ACTION_GLYPH 24
#define HANDLE_H 48
#define CLOSE_DRAG 40
#define HANDLE_BAR_W 48
#define HANDLE_BAR_H 5
#define SCRIM_COLOR 0x080C18
#define SCRIM_OPA 153
#define ANIM_MS 200
#define REFRESH_MS 2000
#define CLOCK_MS 1000
#define TEXT 64

typedef struct {
    lv_obj_t *scrim;
    lv_obj_t *panel;
    lv_obj_t *time;
    lv_obj_t *date;
    lv_obj_t *wifi;
    lv_obj_t *bt;
    lv_obj_t *brightness;
    lv_obj_t *volume;
    lv_obj_t *hub;
    lv_timer_t *clock_timer;
    lv_timer_t *refresh_timer;
    int32_t panel_h;
    bool closing;
} drawer_t;

static drawer_t drawer;

static void update_clock(lv_timer_t *timer)
{
    (void)timer;
    time_t now = time(NULL);
    struct tm local;
    char text[TEXT];
    localtime_r(&now, &local);
    strftime(text, sizeof(text), "%H:%M", &local);
    lv_label_set_text(drawer.time, text);
    strftime(text, sizeof(text), "%A, %-d %B", &local);
    lv_label_set_text(drawer.date, text);
}

static void refresh_tiles(lv_timer_t *timer)
{
    (void)timer;
    net_status_t status;
    char name[BT_NAME_MAX];

    net_status(&status);
    if (status.link == NET_LINK_OFF)
        sui_quick_tile_set(drawer.wifi, false, "Off");
    else if (status.link == NET_LINK_CONNECTED && status.ssid[0])
        sui_quick_tile_set(drawer.wifi, true, status.ssid);
    else
        sui_quick_tile_set(drawer.wifi, true, status.link == NET_LINK_CONNECTING ? "Connecting…" : "On");

    if (!bt_powered())
        sui_quick_tile_set(drawer.bt, false, "Off");
    else
        sui_quick_tile_set(drawer.bt, true, bt_connected_name(name, sizeof(name)) ? name : "On");
}

static void refresh_sliders(void)
{
    int percent;
    bool muted;
    bool has_brightness = backlight_get(&percent);
    if (has_brightness)
        sui_slider_row_set_value(drawer.brightness, percent);
    sui_slider_row_set_available(drawer.brightness, has_brightness);

    bool has_volume = audio_get_volume(AUDIO_OUTPUT, &percent, &muted);
    if (has_volume)
        sui_slider_row_set_value(drawer.volume, percent);
    sui_slider_row_set_available(drawer.volume, has_volume);
}

static void animate(void *obj, int32_t value)
{
    (void)obj;
    lv_obj_set_style_translate_y(drawer.panel, -drawer.panel_h * (100 - value) / 100, 0);
    lv_obj_set_style_bg_opa(drawer.scrim, (lv_opa_t)(SCRIM_OPA * value / 100), 0);
}

static void on_closed(lv_anim_t *anim)
{
    (void)anim;
    lv_obj_delete_async(drawer.scrim);
}

static void on_delete(lv_event_t *e)
{
    (void)e;
    lv_timer_delete(drawer.clock_timer);
    lv_timer_delete(drawer.refresh_timer);
    drawer = (drawer_t){ 0 };
}

static void run_anim(int32_t from, int32_t to, lv_anim_completed_cb_t done)
{
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, drawer.scrim);
    lv_anim_set_values(&anim, from, to);
    lv_anim_set_duration(&anim, ANIM_MS);
    lv_anim_set_exec_cb(&anim, animate);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&anim, done);
    lv_anim_start(&anim);
}

void quick_drawer_close(void)
{
    if (!drawer.scrim || drawer.closing)
        return;
    drawer.closing = true;
    lv_anim_delete(drawer.scrim, animate);
    run_anim(100, 0, on_closed);
}

static void close_now(void)
{
    if (!drawer.scrim)
        return;
    lv_anim_delete(drawer.scrim, animate);
    lv_obj_delete_async(drawer.scrim);
    drawer.closing = true;
}

static void on_close(lv_event_t *e)
{
    (void)e;
    quick_drawer_close();
}

static void on_gesture(lv_event_t *e)
{
    (void)e;
    if (lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_TOP)
        quick_drawer_close();
}

static void open_screen(const sui_screen_def_t *screen)
{
    close_now();
    sui_nav_home();
    sui_nav_push(screen, NULL);
}

static void toggle_wifi(lv_event_t *e)
{
    (void)e;
    bool on = !sui_quick_tile_is_on(drawer.wifi);
    net_wifi_set_enabled(on);
    sui_quick_tile_set(drawer.wifi, on, on ? "On" : "Off");
}

static void toggle_bt(lv_event_t *e)
{
    (void)e;
    bool on = !sui_quick_tile_is_on(drawer.bt);
    bt_set_powered(on);
    sui_quick_tile_set(drawer.bt, on, on ? "On" : "Off");
}

static void open_wifi(lv_event_t *e)
{
    (void)e;
    open_screen(&wifi_screen);
}

static void open_bt(lv_event_t *e)
{
    (void)e;
    open_screen(&bluetooth_screen);
}

static void open_power(lv_event_t *e)
{
    (void)e;
    open_screen(&power_screen);
}

static void open_settings(lv_event_t *e)
{
    (void)e;
    open_screen(&settings_screen);
}

static void open_owner(notify_owner_t owner)
{
    static const sui_screen_def_t *const screens[NOTIFY_OWNER_COUNT] = {
        [NOTIFY_CLOCK] = &clock_app_screen,
        [NOTIFY_UPDATE] = &software_update_screen,
        [NOTIFY_BLUETOOTH] = &bluetooth_screen,
    };
    if (owner < NOTIFY_OWNER_COUNT)
        open_screen(screens[owner]);
}

static void set_brightness(lv_event_t *e)
{
    backlight_set((int)lv_slider_get_value(lv_event_get_target(e)));
}

static void set_volume(lv_event_t *e)
{
    audio_set_volume(AUDIO_OUTPUT, (int)lv_slider_get_value(lv_event_get_target(e)));
}

static lv_obj_t *add_column(lv_obj_t *parent, lv_flex_flow_t flow)
{
    lv_obj_t *column = sui_box_create(parent);
    lv_obj_set_width(column, CONTROLS_W);
    lv_obj_set_style_pad_row(column, CONTROL_GAP, 0);
    lv_obj_set_style_pad_column(column, CONTROL_GAP, 0);
    lv_obj_set_flex_flow(column, flow);
    return column;
}

static void add_action(lv_obj_t *parent, const char *symbol, uint32_t fill, uint32_t ink, lv_event_cb_t cb)
{
    lv_obj_t *button = sui_box_create(parent);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(button, ACTION_SIZE, ACTION_SIZE);
    lv_obj_set_style_radius(button, SUI_RADIUS_ROW, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(fill), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_add_style(button, sui_style_pressed(), LV_STATE_PRESSED);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_center(sui_glyph_create(button, SUI_SYMBOL(symbol), ACTION_GLYPH, ink));
}

static void create_top_row(lv_obj_t *panel)
{
    lv_obj_t *row = sui_box_create(panel);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *clock = sui_box_create(row);
    lv_obj_set_flex_flow(clock, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(clock, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(clock, SUI_SPACE_M, 0);
    drawer.time = lv_label_create(clock);
    lv_obj_set_style_text_font(drawer.time, sui_font(SUI_TEXT_DISPLAY), 0);
    drawer.date = lv_label_create(clock);
    lv_obj_set_style_text_color(drawer.date, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_pad_bottom(drawer.date, SUI_SPACE_S, 0);

    lv_obj_t *actions = sui_box_create(row);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(actions, CONTROL_GAP, 0);
    add_action(actions, LV_SYMBOL_SETTINGS, SUI_COLOR_RAISED, SUI_COLOR_TEXT, open_settings);
    add_action(actions, LV_SYMBOL_POWER, SUI_COLOR_DANGER_FILL, SUI_COLOR_WHITE, open_power);
}

static lv_obj_t *add_slider(lv_obj_t *parent, sui_glyph_t glyph, uint32_t color, int32_t min, lv_event_cb_t cb,
                            lv_event_code_t code)
{
    lv_obj_t *row = sui_slider_row_create(parent, glyph, color, min, 100);
    lv_obj_set_height(row, SLIDER_ROW_H);
    lv_obj_set_style_bg_color(row, lv_color_hex(SUI_COLOR_RAISED), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, SUI_RADIUS_TILE, 0);
    lv_obj_add_event_cb(sui_slider_row_slider(row), cb, code, NULL);
    return row;
}

static void create_controls(lv_obj_t *panel)
{
    lv_obj_t *tiles = add_column(panel, LV_FLEX_FLOW_ROW);
    drawer.wifi = sui_quick_tile_create(tiles, SUI_SYMBOL(LV_SYMBOL_WIFI));
    lv_obj_add_event_cb(drawer.wifi, toggle_wifi, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(drawer.wifi, open_wifi, LV_EVENT_LONG_PRESSED, NULL);
    drawer.bt = sui_quick_tile_create(tiles, SUI_SYMBOL(LV_SYMBOL_BLUETOOTH));
    lv_obj_add_event_cb(drawer.bt, toggle_bt, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(drawer.bt, open_bt, LV_EVENT_LONG_PRESSED, NULL);

    lv_obj_t *sliders = add_column(panel, LV_FLEX_FLOW_COLUMN);
    drawer.brightness = add_slider(sliders, SUI_VECTOR(SUI_ICON_SUN), SUI_COLOR_CAT_DISPLAY, BACKLIGHT_MIN_PERCENT,
                                   set_brightness, LV_EVENT_VALUE_CHANGED);
    drawer.volume = add_slider(sliders, SUI_SYMBOL(LV_SYMBOL_VOLUME_MAX), SUI_COLOR_ACCENT, 0, set_volume,
                               LV_EVENT_RELEASED);
}

static void layout_panel(void)
{
    bool has_items = notify_items()->count > 0;
    lv_obj_set_height(drawer.panel, has_items ? SUI_SCREEN_H + PANEL_RADIUS : LV_SIZE_CONTENT);
    lv_obj_set_height(drawer.hub, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(drawer.hub, has_items ? 1 : 0);
    lv_obj_update_layout(drawer.scrim);
    drawer.panel_h = lv_obj_get_height(drawer.panel);
}

static void on_notify(void)
{
    if (!drawer.hub || drawer.closing)
        return;
    notification_hub_refresh(drawer.hub);
    layout_panel();
}

static void create_hub(lv_obj_t *panel)
{
    drawer.hub = notification_hub_create(panel, open_owner);
    lv_obj_set_width(drawer.hub, CONTROLS_W);
}

static void on_drag(lv_event_t *e)
{
    static int32_t start_y;
    lv_point_t point;
    lv_indev_get_point(lv_indev_active(), &point);
    if (lv_event_get_code(e) == LV_EVENT_PRESSED)
        start_y = point.y;
    else if (start_y - point.y > CLOSE_DRAG)
        quick_drawer_close();
}

static void add_drag_to_close(lv_obj_t *obj)
{
    lv_obj_add_event_cb(obj, on_drag, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(obj, on_drag, LV_EVENT_PRESSING, NULL);
}

static void create_handle(lv_obj_t *panel)
{
    lv_obj_t *handle = sui_box_create(panel);
    lv_obj_add_flag(handle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(handle, LV_PCT(100), HANDLE_H);
    lv_obj_add_event_cb(handle, on_close, LV_EVENT_CLICKED, NULL);
    add_drag_to_close(handle);
    lv_obj_t *bar = sui_box_create(handle);
    lv_obj_set_size(bar, HANDLE_BAR_W, HANDLE_BAR_H);
    lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(SUI_COLOR_TRACK), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_center(bar);
}

void quick_drawer_open(void)
{
    static bool listening;
    if (drawer.scrim)
        return;
    if (!listening) {
        notify_on_change(on_notify);
        listening = true;
    }
    notify_mark_seen();

    drawer.scrim = sui_box_create(lv_layer_top());
    lv_obj_add_flag(drawer.scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(drawer.scrim, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(drawer.scrim, lv_color_hex(SCRIM_COLOR), 0);
    lv_obj_add_event_cb(drawer.scrim, on_close, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(drawer.scrim, on_delete, LV_EVENT_DELETE, NULL);

    drawer.panel = sui_box_create(drawer.scrim);
    lv_obj_add_flag(drawer.panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_width(drawer.panel, LV_PCT(100));
    lv_obj_set_y(drawer.panel, -PANEL_RADIUS);
    lv_obj_set_style_pad_top(drawer.panel, PANEL_RADIUS + PANEL_PAD_TOP, 0);
    lv_obj_set_style_pad_bottom(drawer.panel, PANEL_PAD_BOTTOM, 0);
    lv_obj_set_style_pad_hor(drawer.panel, SUI_SPACE_XL, 0);
    lv_obj_set_style_pad_row(drawer.panel, PANEL_GAP, 0);
    lv_obj_set_style_radius(drawer.panel, PANEL_RADIUS, 0);
    lv_obj_set_style_text_color(drawer.panel, lv_color_hex(SUI_COLOR_TEXT), 0);
    lv_obj_set_style_bg_color(drawer.panel, lv_color_hex(SUI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(drawer.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(drawer.panel, lv_color_hex(SUI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(drawer.panel, 1, 0);
    lv_obj_set_style_border_side(drawer.panel, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_flex_flow(drawer.panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(drawer.panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(drawer.panel, on_gesture, LV_EVENT_GESTURE, NULL);
    add_drag_to_close(drawer.panel);

    create_top_row(drawer.panel);
    create_controls(drawer.panel);
    create_hub(drawer.panel);
    create_handle(drawer.panel);

    update_clock(NULL);
    refresh_tiles(NULL);
    refresh_sliders();
    drawer.clock_timer = lv_timer_create(update_clock, CLOCK_MS, NULL);
    drawer.refresh_timer = lv_timer_create(refresh_tiles, REFRESH_MS, NULL);

    layout_panel();
    animate(NULL, 0);
    run_anim(0, 100, NULL);
}
