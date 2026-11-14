#ifndef SLATE_COMPOSITOR_SERVER_H
#define SLATE_COMPOSITOR_SERVER_H

#ifndef WLR_USE_UNSTABLE
#define WLR_USE_UNSTABLE
#endif

#include <pixman.h>
#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_shell.h>

#include "access.h"
#include "lvgl.h"
#include "platform/platform.h"
#include "theme/theme.h"

#define SERVER_W SUI_SCREEN_W
#define SERVER_H SUI_SCREEN_H
#define SERVER_APP_Y SUI_STATUS_BAR_H
#define SHELL_TOUCH_QUEUE 32

/* A shell-drawn layer: an LVGL display rendering into a CPU buffer that sits in the scene. */
typedef struct {
    struct wlr_buffer base;
    lv_display_t *display;
    lv_indev_t *indev;
    struct wlr_scene_buffer *node;
    uint8_t *pixels;
    uint32_t format;
    size_t stride;
    bool premultiply;
    pixman_region32_t damage;
    int32_t x, y;
    bool pressed;
    /* Touch changes waiting for LVGL's next read, oldest first, so taps shorter than LVGL's
     * read period still deliver both the press and the release. */
    struct {
        int32_t x, y;
        bool pressed;
    } queue[SHELL_TOUCH_QUEUE];
    size_t queued;
} shell_layer_t;

typedef struct window window_t;

typedef struct {
    struct wl_display *display;
    struct wl_event_loop *loop;
    struct wlr_backend *backend;
    struct wlr_session *session;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;
    struct wlr_scene *scene;
    struct wlr_output_layout *layout;
    struct wlr_scene_output_layout *scene_layout;
    struct wlr_output *output;
    int rotation;
    struct wlr_seat *seat;
    struct wlr_keyboard *keyboard;
    struct wlr_xdg_shell *xdg;
    struct wlr_layer_shell_v1 *layer_shell;
    struct wlr_xdg_activation_v1 *activation;
    struct wlr_scene_tree *apps;
    struct wlr_scene_tree *overlays;
    shell_layer_t base;
    shell_layer_t system;
    window_t *front;
    bool want_shot;
} server_t;

extern server_t server;

bool shell_layer_init(shell_layer_t *layer, bool alpha);
void shell_layer_touch(shell_layer_t *layer, int32_t x, int32_t y, bool pressed);

void windows_init(void);
struct wlr_surface *windows_surface_at(double x, double y, double *sx, double *sy);
struct wlr_surface *windows_front_surface(void);

void overlays_init(void);
bool overlays_any(void);
struct wlr_surface *overlays_surface_at(double x, double y, double *sx, double *sy);
void overlays_dismiss(void);

void input_init(void);
void input_add_device(struct wlr_input_device *device);
void input_focus_changed(void);

/* The app a Wayland client belongs to, from the user it runs as. False for anything else. */
bool server_client_app(const struct wl_client *client, char *id, size_t len);
bool server_client_permitted(const struct wl_client *client, const char *permission);

#endif
