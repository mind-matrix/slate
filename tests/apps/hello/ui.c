/* Hello's window and overlay: a test client for Slate's compositor.
 *
 * With gui it opens a full-screen window that changes colour on every touch. It handles Back
 * the way APPS.md asks: close unmaps the window, and the activated state maps it again. With
 * overlay, SIGUSR1 shows a panel near the bottom of the screen until Slate closes it. Every
 * event is logged to stderr, which lands in the journal. */
#define _GNU_SOURCE
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <wayland-client.h>

#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#define OVERLAY_W 800
#define OVERLAY_H 160
#define OVERLAY_MARGIN 40

static const uint32_t colours[] = { 0xFF1E6F5C, 0xFF29339B, 0xFF8E3B46, 0xFF6B4E16 };

typedef struct {
    struct wl_surface *surface;
    struct wl_buffer *buffer;
    int width, height;
} canvas_t;

static struct {
    struct wl_display *display;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct wl_touch *touch;
    struct xdg_wm_base *wm;
    struct zwlr_layer_shell_v1 *layer_shell;
    canvas_t window;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *toplevel;
    bool window_mapped;
    bool window_activated;
    int touches;
    canvas_t overlay;
    struct zwlr_layer_surface_v1 *layer;
} hello;

static struct wl_buffer *paint(int width, int height, uint32_t colour, bool rounded)
{
    int stride = width * 4, size = stride * height;
    int fd = memfd_create("hello", MFD_CLOEXEC);
    if (fd < 0 || ftruncate(fd, size) < 0)
        return NULL;
    uint32_t *px = mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (px == MAP_FAILED) {
        close(fd);
        return NULL;
    }
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            bool corner = rounded && ((x < 24 || x >= width - 24) && (y < 24 || y >= height - 24));
            bool marker = x >= 40 && x < 160 && y >= 40 && y < 160;
            px[y * width + x] = corner ? 0 : marker ? 0xFFFFFFFF : colour;
        }
    munmap(px, (size_t)size);
    struct wl_shm_pool *pool = wl_shm_create_pool(hello.shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    return buffer;
}

static void draw(canvas_t *canvas, uint32_t colour, bool rounded)
{
    if (canvas->buffer)
        wl_buffer_destroy(canvas->buffer);
    canvas->buffer = paint(canvas->width, canvas->height, colour, rounded);
    wl_surface_attach(canvas->surface, canvas->buffer, 0, 0);
    wl_surface_damage_buffer(canvas->surface, 0, 0, canvas->width, canvas->height);
    wl_surface_commit(canvas->surface);
}

/* ---- window ---------------------------------------------------------------------------- */

static void xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial)
{
    (void)data;
    xdg_surface_ack_configure(surface, serial);
    if (!hello.window_mapped && hello.window_activated) {
        hello.window_mapped = true;
        fprintf(stderr, "hello: window shown %dx%d\n", hello.window.width, hello.window.height);
    }
    if (hello.window_mapped)
        draw(&hello.window, colours[hello.touches % 4], false);
    else
        wl_surface_commit(hello.window.surface);
}

static const struct xdg_surface_listener xdg_surface_listener = { xdg_surface_configure };

static void toplevel_configure(void *data, struct xdg_toplevel *toplevel, int32_t width, int32_t height,
                               struct wl_array *states)
{
    (void)data;
    (void)toplevel;
    if (width > 0 && height > 0) {
        hello.window.width = width;
        hello.window.height = height;
    }
    bool activated = false;
    uint32_t *state;
    wl_array_for_each(state, states) if (*state == XDG_TOPLEVEL_STATE_ACTIVATED) activated = true;
    hello.window_activated = activated;
}

/* Back. Hello has nothing to go back to, so it unmaps its window and keeps running. */
static void toplevel_close(void *data, struct xdg_toplevel *toplevel)
{
    (void)data;
    (void)toplevel;
    fprintf(stderr, "hello: back, hiding the window\n");
    hello.window_mapped = false;
    hello.window_activated = false;
    wl_surface_attach(hello.window.surface, NULL, 0, 0);
    wl_surface_commit(hello.window.surface);
    /* Unmapping resets the window; this commit starts it over, so Slate can ask for it again. */
    wl_surface_commit(hello.window.surface);
}

static const struct xdg_toplevel_listener toplevel_listener = { .configure = toplevel_configure, .close = toplevel_close };

static void create_window(void)
{
    hello.window.width = 1280;
    hello.window.height = 672;
    hello.window.surface = wl_compositor_create_surface(hello.compositor);
    hello.xdg_surface = xdg_wm_base_get_xdg_surface(hello.wm, hello.window.surface);
    xdg_surface_add_listener(hello.xdg_surface, &xdg_surface_listener, NULL);
    hello.toplevel = xdg_surface_get_toplevel(hello.xdg_surface);
    xdg_toplevel_add_listener(hello.toplevel, &toplevel_listener, NULL);
    xdg_toplevel_set_title(hello.toplevel, "Hello");
    xdg_toplevel_set_app_id(hello.toplevel, "hello");
    /* Map straight away: Slate keeps the window hidden until the app is opened. */
    hello.window_activated = true;
    wl_surface_commit(hello.window.surface);
}

/* ---- overlay --------------------------------------------------------------------------- */

static void layer_configure(void *data, struct zwlr_layer_surface_v1 *layer, uint32_t serial, uint32_t width,
                            uint32_t height)
{
    (void)data;
    zwlr_layer_surface_v1_ack_configure(layer, serial);
    hello.overlay.width = width ? (int)width : OVERLAY_W;
    hello.overlay.height = height ? (int)height : OVERLAY_H;
    draw(&hello.overlay, 0xF0253052, true);
}

static void destroy_overlay(void)
{
    if (!hello.layer)
        return;
    zwlr_layer_surface_v1_destroy(hello.layer);
    wl_surface_destroy(hello.overlay.surface);
    if (hello.overlay.buffer)
        wl_buffer_destroy(hello.overlay.buffer);
    memset(&hello.overlay, 0, sizeof(hello.overlay));
    hello.layer = NULL;
}

static void layer_closed(void *data, struct zwlr_layer_surface_v1 *layer)
{
    (void)data;
    (void)layer;
    fprintf(stderr, "hello: overlay closed by Slate\n");
    destroy_overlay();
}

static const struct zwlr_layer_surface_v1_listener layer_listener = { layer_configure, layer_closed };

static void show_overlay(void)
{
    if (!hello.layer_shell) {
        fprintf(stderr, "hello: no overlay permission\n");
        return;
    }
    if (hello.layer)
        return;
    hello.overlay.surface = wl_compositor_create_surface(hello.compositor);
    hello.layer = zwlr_layer_shell_v1_get_layer_surface(hello.layer_shell, hello.overlay.surface, NULL,
                                                        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "hello-overlay");
    zwlr_layer_surface_v1_add_listener(hello.layer, &layer_listener, NULL);
    zwlr_layer_surface_v1_set_size(hello.layer, OVERLAY_W, OVERLAY_H);
    zwlr_layer_surface_v1_set_anchor(hello.layer, ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM);
    zwlr_layer_surface_v1_set_margin(hello.layer, 0, 0, OVERLAY_MARGIN, 0);
    wl_surface_commit(hello.overlay.surface);
    fprintf(stderr, "hello: overlay shown\n");
}

/* ---- touch ----------------------------------------------------------------------------- */

static void touch_down(void *data, struct wl_touch *touch, uint32_t serial, uint32_t time, struct wl_surface *surface,
                       int32_t id, wl_fixed_t x, wl_fixed_t y)
{
    (void)data;
    (void)touch;
    (void)serial;
    (void)time;
    (void)id;
    bool on_window = surface == hello.window.surface;
    fprintf(stderr, "hello: touch on the %s at %.0f,%.0f\n", on_window ? "window" : "overlay", wl_fixed_to_double(x),
            wl_fixed_to_double(y));
    if (on_window && hello.window_mapped) {
        hello.touches++;
        draw(&hello.window, colours[hello.touches % 4], false);
    }
}

static void touch_up(void *d, struct wl_touch *t, uint32_t s, uint32_t time, int32_t id)
{
    (void)d, (void)t, (void)s, (void)time, (void)id;
}

static void touch_motion(void *d, struct wl_touch *t, uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y)
{
    (void)d, (void)t, (void)time, (void)id, (void)x, (void)y;
}

static void touch_frame(void *d, struct wl_touch *t)
{
    (void)d, (void)t;
}

static void touch_cancel(void *d, struct wl_touch *t)
{
    (void)d, (void)t;
}

static void touch_shape(void *d, struct wl_touch *t, int32_t id, wl_fixed_t major, wl_fixed_t minor)
{
    (void)d, (void)t, (void)id, (void)major, (void)minor;
}

static void touch_orientation(void *d, struct wl_touch *t, int32_t id, wl_fixed_t orientation)
{
    (void)d, (void)t, (void)id, (void)orientation;
}

static const struct wl_touch_listener touch_listener = {
    touch_down, touch_up, touch_motion, touch_frame, touch_cancel, touch_shape, touch_orientation,
};

static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps)
{
    (void)data;
    if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !hello.touch) {
        hello.touch = wl_seat_get_touch(seat);
        wl_touch_add_listener(hello.touch, &touch_listener, NULL);
    }
}

static void seat_name(void *data, struct wl_seat *seat, const char *name)
{
    (void)data, (void)seat, (void)name;
}

static const struct wl_seat_listener seat_listener = { seat_capabilities, seat_name };

/* ---- setup ----------------------------------------------------------------------------- */

static void wm_ping(void *data, struct xdg_wm_base *wm, uint32_t serial)
{
    (void)data;
    xdg_wm_base_pong(wm, serial);
}

static const struct xdg_wm_base_listener wm_listener = { wm_ping };

static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    (void)data;
    (void)version;
    if (strcmp(interface, wl_compositor_interface.name) == 0)
        hello.compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    else if (strcmp(interface, wl_shm_interface.name) == 0)
        hello.shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    else if (strcmp(interface, wl_seat_interface.name) == 0) {
        hello.seat = wl_registry_bind(registry, name, &wl_seat_interface, 5);
        wl_seat_add_listener(hello.seat, &seat_listener, NULL);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        hello.wm = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(hello.wm, &wm_listener, NULL);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0)
        hello.layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
}

static void global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    (void)data, (void)registry, (void)name;
}

static const struct wl_registry_listener registry_listener = { global, global_remove };

int main(void)
{
    hello.display = wl_display_connect(NULL);
    if (!hello.display) {
        fprintf(stderr, "hello: no display (%s)\n", strerror(errno));
        return 1;
    }
    struct wl_registry *registry = wl_display_get_registry(hello.display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(hello.display);
    wl_display_roundtrip(hello.display);
    fprintf(stderr, "hello: connected, window %s, overlay %s\n", hello.wm ? "allowed" : "not allowed",
            hello.layer_shell ? "allowed" : "not allowed");
    if (!hello.compositor || !hello.shm)
        return 1;
    if (hello.wm)
        create_window();

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    int sfd = signalfd(-1, &mask, SFD_CLOEXEC);
    struct pollfd fds[2] = { { wl_display_get_fd(hello.display), POLLIN, 0 }, { sfd, POLLIN, 0 } };
    for (;;) {
        wl_display_flush(hello.display);
        if (poll(fds, 2, -1) < 0 && errno != EINTR)
            break;
        if (fds[1].revents & POLLIN) {
            struct signalfd_siginfo info;
            if (read(sfd, &info, sizeof(info)) == (ssize_t)sizeof(info))
                show_overlay();
        }
        if ((fds[0].revents & POLLIN) && wl_display_dispatch(hello.display) < 0)
            break;
        if (fds[0].revents & (POLLHUP | POLLERR))
            break;
    }
    fprintf(stderr, "hello: display connection closed\n");
    return 0;
}
