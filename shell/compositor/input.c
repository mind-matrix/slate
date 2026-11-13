#include "server.h"

#include <linux/input-event-codes.h>
#include <stdlib.h>
#include <xkbcommon/xkbcommon.h>

#include "geometry.h"
#include "platform/backend.h"

#define REPEAT_DELAY_MS 500
#define REPEAT_INTERVAL_MS 40
#define CLIENT_REPEAT_HZ 25
#define CLIENT_REPEAT_DELAY_MS 600

typedef enum {
    TARGET_NONE,
    TARGET_SHELL,
    TARGET_CLIENT,
} target_t;

typedef struct {
    struct wlr_touch *touch;
    struct wl_listener down;
    struct wl_listener up;
    struct wl_listener motion;
    struct wl_listener cancel;
    struct wl_listener frame;
    struct wl_listener destroy;
} touch_device_t;

typedef struct {
    struct wlr_keyboard *keyboard;
    struct wl_listener key;
    struct wl_listener modifiers;
    struct wl_listener destroy;
} key_device_t;

static struct {
    target_t target;
    int points;
    int32_t primary;
    shell_layer_t *layer;
    struct wlr_surface *surface;
    double origin_x;
    double origin_y;
    struct wl_listener surface_destroy;
    struct wl_event_source *repeat;
    uint32_t repeat_code;
    bool has_keyboard;
} input;

/* ---- touch ------------------------------------------------------------------------------ */

static void to_layout(double nx, double ny, double *x, double *y)
{
    geometry_touch(server.rotation, nx, ny, SERVER_W, SERVER_H, x, y);
}

static bool system_hit(double x, double y)
{
    lv_obj_t *top = lv_display_get_layer_top(server.system.display);
    lv_point_t p = { (int32_t)x, (int32_t)y };
    lv_obj_t *found = lv_indev_search_obj(top, &p);
    return found && found != top;
}

static void on_surface_destroy(struct wl_listener *listener, void *data)
{
    (void)listener;
    (void)data;
    input.surface = NULL;
    wl_list_remove(&input.surface_destroy.link);
    wl_list_init(&input.surface_destroy.link);
}

static void target_surface(struct wlr_surface *surface, double x, double y, double sx, double sy)
{
    input.target = TARGET_CLIENT;
    input.surface = surface;
    input.origin_x = x - sx;
    input.origin_y = y - sy;
    wl_list_remove(&input.surface_destroy.link);
    wl_signal_add(&surface->events.destroy, &input.surface_destroy);
}

static void choose_target(double x, double y)
{
    double sx, sy;
    struct wlr_surface *surface;
    input.target = TARGET_NONE;
    input.layer = NULL;
    if (sui_wake())
        return;
    if (system_hit(x, y)) {
        input.target = TARGET_SHELL;
        input.layer = &server.system;
    } else if (overlays_any()) {
        if ((surface = overlays_surface_at(x, y, &sx, &sy)))
            target_surface(surface, x, y, sx, sy);
        else
            overlays_dismiss();
    } else if (server.front) {
        if ((surface = windows_surface_at(x, y, &sx, &sy)))
            target_surface(surface, x, y, sx, sy);
    } else {
        input.target = TARGET_SHELL;
        input.layer = &server.base;
    }
}

static void shell_point(double x, double y, bool pressed)
{
    input.layer->x = (int32_t)x;
    input.layer->y = (int32_t)y;
    input.layer->pressed = pressed;
    lv_indev_read(input.layer->indev);
}

static void on_touch_down(struct wl_listener *listener, void *data)
{
    (void)listener;
    const struct wlr_touch_down_event *event = data;
    double x, y;
    to_layout(event->x, event->y, &x, &y);
    lv_display_trigger_activity(server.base.display);
    if (input.points++ == 0) {
        choose_target(x, y);
        input.primary = event->touch_id;
    }
    if (input.target == TARGET_SHELL && event->touch_id == input.primary)
        shell_point(x, y, true);
    else if (input.target == TARGET_CLIENT && input.surface)
        wlr_seat_touch_notify_down(server.seat, input.surface, event->time_msec, event->touch_id,
                                   x - input.origin_x, y - input.origin_y);
}

static void on_touch_motion(struct wl_listener *listener, void *data)
{
    (void)listener;
    const struct wlr_touch_motion_event *event = data;
    double x, y;
    to_layout(event->x, event->y, &x, &y);
    lv_display_trigger_activity(server.base.display);
    if (input.target == TARGET_SHELL && event->touch_id == input.primary)
        shell_point(x, y, true);
    else if (input.target == TARGET_CLIENT)
        wlr_seat_touch_notify_motion(server.seat, event->time_msec, event->touch_id, x - input.origin_x,
                                     y - input.origin_y);
}

static void release(int32_t touch_id, uint32_t time_msec)
{
    if (input.target == TARGET_SHELL && touch_id == input.primary)
        shell_point(input.layer->x, input.layer->y, false);
    else if (input.target == TARGET_CLIENT)
        wlr_seat_touch_notify_up(server.seat, time_msec, touch_id);
    if (input.points > 0 && --input.points == 0)
        input.target = TARGET_NONE;
}

static void on_touch_up(struct wl_listener *listener, void *data)
{
    (void)listener;
    const struct wlr_touch_up_event *event = data;
    release(event->touch_id, event->time_msec);
}

static void on_touch_cancel(struct wl_listener *listener, void *data)
{
    (void)listener;
    const struct wlr_touch_cancel_event *event = data;
    release(event->touch_id, event->time_msec);
}

static void on_touch_frame(struct wl_listener *listener, void *data)
{
    (void)listener;
    (void)data;
    if (input.target == TARGET_CLIENT)
        wlr_seat_touch_notify_frame(server.seat);
}

static void on_touch_destroy(struct wl_listener *listener, void *data)
{
    (void)data;
    touch_device_t *device = wl_container_of(listener, device, destroy);
    wl_list_remove(&device->down.link);
    wl_list_remove(&device->up.link);
    wl_list_remove(&device->motion.link);
    wl_list_remove(&device->cancel.link);
    wl_list_remove(&device->frame.link);
    wl_list_remove(&device->destroy.link);
    free(device);
}

static void add_touch(struct wlr_input_device *wlr)
{
    touch_device_t *device = calloc(1, sizeof(*device));
    if (!device)
        return;
    device->touch = wlr_touch_from_input_device(wlr);
    device->down.notify = on_touch_down;
    wl_signal_add(&device->touch->events.down, &device->down);
    device->up.notify = on_touch_up;
    wl_signal_add(&device->touch->events.up, &device->up);
    device->motion.notify = on_touch_motion;
    wl_signal_add(&device->touch->events.motion, &device->motion);
    device->cancel.notify = on_touch_cancel;
    wl_signal_add(&device->touch->events.cancel, &device->cancel);
    device->frame.notify = on_touch_frame;
    wl_signal_add(&device->touch->events.frame, &device->frame);
    device->destroy.notify = on_touch_destroy;
    wl_signal_add(&wlr->events.destroy, &device->destroy);
}

/* ---- keyboard --------------------------------------------------------------------------- */

static bool is_modifier(uint32_t code)
{
    switch (code) {
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
    case KEY_CAPSLOCK:
        return true;
    default:
        return false;
    }
}

static int on_repeat(void *data)
{
    (void)data;
    sui_keyboard_key(input.repeat_code, 2);
    wl_event_source_timer_update(input.repeat, REPEAT_INTERVAL_MS);
    return 0;
}

static void on_key(struct wl_listener *listener, void *data)
{
    key_device_t *device = wl_container_of(listener, device, key);
    const struct wlr_keyboard_key_event *event = data;
    lv_display_trigger_activity(server.base.display);
    if (windows_front_surface()) {
        wlr_seat_set_keyboard(server.seat, device->keyboard);
        wlr_seat_keyboard_notify_key(server.seat, event->time_msec, event->keycode, event->state);
        return;
    }
    bool down = event->state == WL_KEYBOARD_KEY_STATE_PRESSED;
    sui_keyboard_key(event->keycode, down ? 1 : 0);
    if (down && !is_modifier(event->keycode)) {
        input.repeat_code = event->keycode;
        wl_event_source_timer_update(input.repeat, REPEAT_DELAY_MS);
    } else if (!down && event->keycode == input.repeat_code) {
        wl_event_source_timer_update(input.repeat, 0);
    }
}

static void on_modifiers(struct wl_listener *listener, void *data)
{
    (void)data;
    key_device_t *device = wl_container_of(listener, device, modifiers);
    if (!windows_front_surface())
        return;
    wlr_seat_set_keyboard(server.seat, device->keyboard);
    wlr_seat_keyboard_notify_modifiers(server.seat, &device->keyboard->modifiers);
}

static void on_keyboard_destroy(struct wl_listener *listener, void *data)
{
    (void)data;
    key_device_t *device = wl_container_of(listener, device, destroy);
    if (server.keyboard == device->keyboard)
        server.keyboard = NULL;
    wl_list_remove(&device->key.link);
    wl_list_remove(&device->modifiers.link);
    wl_list_remove(&device->destroy.link);
    free(device);
}

static void add_keyboard(struct wlr_input_device *wlr)
{
    key_device_t *device = calloc(1, sizeof(*device));
    if (!device)
        return;
    device->keyboard = wlr_keyboard_from_input_device(wlr);
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap = context ? xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS) : NULL;
    if (keymap)
        wlr_keyboard_set_keymap(device->keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    wlr_keyboard_set_repeat_info(device->keyboard, CLIENT_REPEAT_HZ, CLIENT_REPEAT_DELAY_MS);
    device->key.notify = on_key;
    wl_signal_add(&device->keyboard->events.key, &device->key);
    device->modifiers.notify = on_modifiers;
    wl_signal_add(&device->keyboard->events.modifiers, &device->modifiers);
    device->destroy.notify = on_keyboard_destroy;
    wl_signal_add(&wlr->events.destroy, &device->destroy);
    server.keyboard = device->keyboard;
    wlr_seat_set_keyboard(server.seat, device->keyboard);
    input.has_keyboard = true;
}

/* ---- setup ------------------------------------------------------------------------------ */

void input_add_device(struct wlr_input_device *device)
{
    if (device->type == WLR_INPUT_DEVICE_TOUCH)
        add_touch(device);
    else if (device->type == WLR_INPUT_DEVICE_KEYBOARD)
        add_keyboard(device);
    wlr_seat_set_capabilities(server.seat,
                              WL_SEAT_CAPABILITY_TOUCH | (input.has_keyboard ? WL_SEAT_CAPABILITY_KEYBOARD : 0));
}

void input_focus_changed(void)
{
    wl_event_source_timer_update(input.repeat, 0);
    struct wlr_surface *surface = windows_front_surface();
    struct wlr_keyboard *keyboard = server.keyboard;
    if (surface && keyboard)
        wlr_seat_keyboard_notify_enter(server.seat, surface, keyboard->keycodes, keyboard->num_keycodes,
                                       &keyboard->modifiers);
    else
        wlr_seat_keyboard_notify_clear_focus(server.seat);
}

void input_init(void)
{
    wl_list_init(&input.surface_destroy.link);
    input.surface_destroy.notify = on_surface_destroy;
    input.repeat = wl_event_loop_add_timer(server.loop, on_repeat, NULL);
}
