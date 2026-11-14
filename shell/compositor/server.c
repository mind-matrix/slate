#include "server.h"

#include <drm_fourcc.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/util/log.h>

#include "geometry.h"
#include "platform/backend.h"

#define SOCKET_PATH "/run/slate-display/wayland-0"
#define SOCKET_DIR "/run/slate-display"
#define SOCKET_GROUP "slate-display"
#define SOCKET_BACKLOG 64
#define OUTPUT_PREFIX "DSI"
#define COMPOSITOR_VERSION 5
#define XDG_SHELL_VERSION 3
#define LAYER_SHELL_VERSION 4
#define SHOT_NAME "slate-screen.ppm"
#define PERMISSIONS_BUF 512

server_t server;

static struct wl_listener new_output;
static struct wl_listener new_input;
static struct wl_listener output_frame;

/* ---- shell layers ---------------------------------------------------------------------- */

static void layer_buffer_destroy(struct wlr_buffer *buffer)
{
    (void)buffer;
}

static bool layer_buffer_begin(struct wlr_buffer *buffer, uint32_t flags, void **data, uint32_t *format,
                               size_t *stride)
{
    (void)flags;
    shell_layer_t *layer = wl_container_of(buffer, layer, base);
    *data = layer->pixels;
    *format = layer->format;
    *stride = layer->stride;
    return true;
}

static void layer_buffer_end(struct wlr_buffer *buffer)
{
    (void)buffer;
}

static const struct wlr_buffer_impl layer_buffer_impl = {
    .destroy = layer_buffer_destroy,
    .begin_data_ptr_access = layer_buffer_begin,
    .end_data_ptr_access = layer_buffer_end,
};

/* LVGL draws straight alpha; the compositor blends premultiplied alpha. LVGL clears and fully
 * redraws every area it refreshes, so converting the flushed area in place is safe. */
static void premultiply(shell_layer_t *layer, const lv_area_t *area)
{
    for (int32_t y = area->y1; y <= area->y2; y++) {
        uint8_t *px = layer->pixels + (size_t)y * layer->stride + (size_t)area->x1 * 4;
        for (int32_t x = area->x1; x <= area->x2; x++, px += 4) {
            uint32_t a = px[3];
            if (a == 255)
                continue;
            px[0] = (uint8_t)((px[0] * a + 127) / 255);
            px[1] = (uint8_t)((px[1] * a + 127) / 255);
            px[2] = (uint8_t)((px[2] * a + 127) / 255);
        }
    }
}

static void layer_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    (void)px_map;
    shell_layer_t *layer = lv_display_get_driver_data(display);
    if (layer->premultiply)
        premultiply(layer, area);
    pixman_region32_union_rect(&layer->damage, &layer->damage, area->x1, area->y1,
                               (unsigned)lv_area_get_width(area), (unsigned)lv_area_get_height(area));
    if (lv_display_flush_is_last(display)) {
        wlr_scene_buffer_set_buffer_with_damage(layer->node, &layer->base, &layer->damage);
        pixman_region32_clear(&layer->damage);
    }
    lv_display_flush_ready(display);
}

void shell_layer_touch(shell_layer_t *layer, int32_t x, int32_t y, bool pressed)
{
    size_t n = layer->queued;
    /* When full, keep the newest change in place of the last one rather than drop it. */
    if (n == SHELL_TOUCH_QUEUE)
        n--;
    layer->queue[n].x = x;
    layer->queue[n].y = y;
    layer->queue[n].pressed = pressed;
    layer->queued = n + 1;
}

static void layer_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    shell_layer_t *layer = lv_indev_get_driver_data(indev);
    if (layer->queued) {
        layer->x = layer->queue[0].x;
        layer->y = layer->queue[0].y;
        layer->pressed = layer->queue[0].pressed;
        memmove(layer->queue, layer->queue + 1, --layer->queued * sizeof(layer->queue[0]));
        data->continue_reading = layer->queued > 0;
    }
    data->point.x = layer->x;
    data->point.y = layer->y;
    data->state = layer->pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

bool shell_layer_init(shell_layer_t *layer, bool alpha)
{
    /* 32-bit colour even for the opaque base layer: 16-bit colour bands visibly on wallpaper gradients. */
    lv_color_format_t cf = alpha ? LV_COLOR_FORMAT_ARGB8888 : LV_COLOR_FORMAT_XRGB8888;
    layer->format = alpha ? DRM_FORMAT_ARGB8888 : DRM_FORMAT_XRGB8888;
    layer->premultiply = alpha;
    layer->stride = lv_draw_buf_width_to_stride(SERVER_W, cf);
    size_t size = layer->stride * SERVER_H;
    layer->pixels = aligned_alloc(64, (size + 63) & ~(size_t)63);
    if (!layer->pixels)
        return false;
    memset(layer->pixels, 0, size);
    pixman_region32_init(&layer->damage);
    wlr_buffer_init(&layer->base, &layer_buffer_impl, SERVER_W, SERVER_H);

    layer->display = lv_display_create(SERVER_W, SERVER_H);
    lv_display_set_color_format(layer->display, cf);
    lv_display_set_buffers(layer->display, layer->pixels, NULL, (uint32_t)size, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(layer->display, layer_flush);
    lv_display_set_driver_data(layer->display, layer);

    layer->indev = lv_indev_create();
    lv_indev_set_type(layer->indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(layer->indev, layer_read);
    lv_indev_set_driver_data(layer->indev, layer);
    lv_indev_set_display(layer->indev, layer->display);
    return true;
}

/* ---- clients and permissions ----------------------------------------------------------- */

static bool client_uid(const struct wl_client *client, uid_t *uid)
{
    pid_t pid;
    gid_t gid;
    wl_client_get_credentials((struct wl_client *)client, &pid, uid, &gid);
    return true;
}

bool server_client_app(const struct wl_client *client, char *id, size_t len)
{
    uid_t uid;
    struct passwd pw, *found = NULL;
    char buf[1024];
    return client_uid(client, &uid) && getpwuid_r(uid, &pw, buf, sizeof(buf), &found) == 0 && found &&
           access_app_from_user(found->pw_name, id, len);
}

static bool read_permissions(const char *app, char *out, size_t len)
{
    char path[128];
    snprintf(path, sizeof(path), ACCESS_PERMISSIONS_DIR "/%s.permissions", app);
    out[0] = '\0';
    FILE *file = fopen(path, "r");
    if (!file)
        return false;
    size_t n = fread(out, 1, len - 1, file);
    fclose(file);
    out[n] = '\0';
    return true;
}

bool server_client_permitted(const struct wl_client *client, const char *permission)
{
    char app[ACCESS_ID_MAX], permissions[PERMISSIONS_BUF];
    if (!server_client_app(client, app, sizeof(app)) || !read_permissions(app, permissions, sizeof(permissions)))
        return false;
    char needle[64];
    snprintf(needle, sizeof(needle), " %s ", permission);
    char padded[PERMISSIONS_BUF + 2];
    snprintf(padded, sizeof(padded), " %s ", permissions);
    for (char *p = padded; *p; p++)
        if (*p == '\n' || *p == '\t')
            *p = ' ';
    return strstr(padded, needle) != NULL;
}

/* The shell's own user and root see every global. Apps see what their permissions allow, and
 * every other user sees nothing. */
static bool global_filter(const struct wl_client *client, const struct wl_global *global, void *data)
{
    (void)data;
    uid_t uid;
    client_uid(client, &uid);
    if (uid == 0 || uid == getuid())
        return true;
    char app[ACCESS_ID_MAX], permissions[PERMISSIONS_BUF];
    if (!server_client_app(client, app, sizeof(app)) || !read_permissions(app, permissions, sizeof(permissions)))
        return false;
    return access_global_allowed(wl_global_get_interface(global)->name, permissions);
}

/* ---- outputs and frames ---------------------------------------------------------------- */

static void write_shot(struct wlr_buffer *buffer)
{
    struct wlr_texture *texture = wlr_texture_from_buffer(server.renderer, buffer);
    if (!texture)
        return;
    uint32_t format = wlr_texture_preferred_read_format(texture);
    int w = (int)texture->width, h = (int)texture->height;
    uint8_t *px = malloc((size_t)w * (size_t)h * 4);
    bool ok = px && wlr_texture_read_pixels(texture, &(struct wlr_texture_read_pixels_options){
                                                         .data = px, .format = format, .stride = (uint32_t)w * 4 });
    char path[256];
    const char *dir = getenv("XDG_RUNTIME_DIR");
    snprintf(path, sizeof(path), "%s/" SHOT_NAME, dir ? dir : "/tmp");
    FILE *file = ok ? fopen(path, "wb") : NULL;
    if (file) {
        bool bgr = format == DRM_FORMAT_XBGR8888 || format == DRM_FORMAT_ABGR8888;
        fprintf(file, "P6\n%d %d\n255\n", w, h);
        for (int i = 0; i < w * h; i++) {
            const uint8_t *p = px + (size_t)i * 4;
            uint8_t rgb[3] = { bgr ? p[0] : p[2], p[1], bgr ? p[2] : p[0] };
            fwrite(rgb, 1, sizeof(rgb), file);
        }
        fclose(file);
    }
    free(px);
    wlr_texture_destroy(texture);
}

static void on_output_frame(struct wl_listener *listener, void *data)
{
    (void)listener;
    (void)data;
    struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(server.scene, server.output);
    if (!scene_output)
        return;
    if (server.want_shot) {
        server.want_shot = false;
        struct wlr_output_state state;
        wlr_output_state_init(&state);
        if (wlr_scene_output_build_state(scene_output, &state, NULL) && state.buffer) {
            write_shot(state.buffer);
            wlr_output_commit_state(server.output, &state);
        }
        wlr_output_state_finish(&state);
    } else {
        wlr_scene_output_commit(scene_output, NULL);
    }
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(scene_output, &now);
}

static void on_new_output(struct wl_listener *listener, void *data)
{
    (void)listener;
    struct wlr_output *output = data;
    const char *prefix = getenv("SLATE_OUTPUT");
    if (!prefix)
        prefix = OUTPUT_PREFIX;
    if (server.output || strncmp(output->name, prefix, strlen(prefix)) != 0)
        return;
    wlr_output_init_render(output, server.allocator, server.renderer);

    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    struct wlr_output_mode *mode = wlr_output_preferred_mode(output);
    if (mode)
        wlr_output_state_set_mode(&state, mode);
    wlr_output_state_set_transform(&state, (enum wl_output_transform)geometry_transform(server.rotation));
    bool ok = wlr_output_commit_state(output, &state);
    wlr_output_state_finish(&state);
    if (!ok) {
        fprintf(stderr, "slate: couldn't enable output %s\n", output->name);
        return;
    }

    server.output = output;
    output_frame.notify = on_output_frame;
    wl_signal_add(&output->events.frame, &output_frame);
    struct wlr_output_layout_output *layout_output = wlr_output_layout_add(server.layout, output, 0, 0);
    struct wlr_scene_output *scene_output = wlr_scene_output_create(server.scene, output);
    wlr_scene_output_layout_add_output(server.scene_layout, layout_output, scene_output);
}

static void on_new_input(struct wl_listener *listener, void *data)
{
    (void)listener;
    input_add_device(data);
}

/* ---- screenshot, socket and main loop -------------------------------------------------- */

static int on_screenshot(int signal, void *data)
{
    (void)signal;
    (void)data;
    struct wlr_scene_output *scene_output =
        server.output ? wlr_scene_get_scene_output(server.scene, server.output) : NULL;
    if (!scene_output)
        return 0;
    server.want_shot = true;
    pixman_region32_t all;
    pixman_region32_init_rect(&all, 0, 0, SERVER_W * 2, SERVER_W * 2);
    wlr_damage_ring_add(&scene_output->damage_ring, &all);
    pixman_region32_fini(&all);
    wlr_output_schedule_frame(server.output);
    return 0;
}

static int open_socket(void)
{
    const char *path = getenv("SLATE_WAYLAND_SOCKET");
    if (!path)
        path = SOCKET_PATH;
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    if (strlen(path) >= sizeof(addr.sun_path))
        return -1;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    unlink(path);
    if (fd < 0 || bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(fd, SOCKET_BACKLOG) < 0) {
        perror("slate: wayland socket");
        if (fd >= 0)
            close(fd);
        return -1;
    }
    /* Apps reach the socket through the slate-display group, which gui and overlay grant. */
    struct group *group = getgrnam(SOCKET_GROUP);
    if (group && chown(path, (uid_t)-1, group->gr_gid) == 0 && chmod(path, 0660) == 0 &&
        strcmp(path, SOCKET_PATH) == 0) {
        if (chown(SOCKET_DIR, (uid_t)-1, group->gr_gid) != 0 || chmod(SOCKET_DIR, 0750) != 0)
            perror("slate: socket directory");
    } else if (chmod(path, 0600) != 0) {
        perror("slate: socket permissions");
    }
    return fd;
}

static void dispatch(int fd, void *user)
{
    (void)fd;
    (void)user;
    wl_event_loop_dispatch(server.loop, 0);
}

/* Runs before the main loop waits. wlroots queues output frames as idle callbacks, which never wake
 * the loop on their own, so run them here or a finished frame waits for the next input event. */
static void flush_clients(void)
{
    wl_event_loop_dispatch_idle(server.loop);
    wl_display_flush_clients(server.display);
}

static int rotation_from_env(void)
{
    const char *value = getenv("SLATE_ROTATION");
    return value ? atoi(value) : 270;
}

static bool start(void)
{
    wlr_log_init(getenv("SLATE_WLR_DEBUG") ? WLR_DEBUG : WLR_ERROR, NULL);
    setenv("LIBSEAT_BACKEND", "noop", 0);
    server.rotation = rotation_from_env();
    server.display = wl_display_create();
    server.loop = wl_display_get_event_loop(server.display);
    server.backend = wlr_backend_autocreate(server.loop, &server.session);
    if (!server.backend)
        return false;
    server.renderer = wlr_renderer_autocreate(server.backend);
    if (!server.renderer)
        return false;
    wlr_renderer_init_wl_display(server.renderer, server.display);
    server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
    if (!server.allocator)
        return false;

    wl_display_set_global_filter(server.display, global_filter, NULL);
    wlr_compositor_create(server.display, COMPOSITOR_VERSION, server.renderer);
    wlr_subcompositor_create(server.display);
    server.layout = wlr_output_layout_create(server.display);
    server.scene = wlr_scene_create();
    server.scene_layout = wlr_scene_attach_output_layout(server.scene, server.layout);
    server.seat = wlr_seat_create(server.display, "seat0");
    server.xdg = wlr_xdg_shell_create(server.display, XDG_SHELL_VERSION);
    server.layer_shell = wlr_layer_shell_v1_create(server.display, LAYER_SHELL_VERSION);
    server.activation = wlr_xdg_activation_v1_create(server.display);

    /* Stacking, bottom to top: the shell's screens, app windows, overlays, then system UI. */
    if (!shell_layer_init(&server.base, false))
        return false;
    server.base.node = wlr_scene_buffer_create(&server.scene->tree, &server.base.base);
    server.apps = wlr_scene_tree_create(&server.scene->tree);
    server.overlays = wlr_scene_tree_create(&server.scene->tree);
    if (!shell_layer_init(&server.system, true))
        return false;
    server.system.node = wlr_scene_buffer_create(&server.scene->tree, &server.system.base);
    lv_display_set_default(server.base.display);
    lv_obj_t *system_screen = lv_display_get_screen_active(server.system.display);
    lv_obj_set_style_bg_opa(system_screen, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(system_screen, LV_OBJ_FLAG_CLICKABLE);

    windows_init();
    overlays_init();
    input_init();

    new_output.notify = on_new_output;
    wl_signal_add(&server.backend->events.new_output, &new_output);
    new_input.notify = on_new_input;
    wl_signal_add(&server.backend->events.new_input, &new_input);
    wl_event_loop_add_signal(server.loop, SIGUSR1, on_screenshot, NULL);

    int fd = open_socket();
    if (fd < 0 || wl_display_add_socket_fd(server.display, fd) < 0)
        return false;
    if (!wlr_backend_start(server.backend))
        return false;
    if (!server.output) {
        fprintf(stderr, "slate: no %s output found\n", getenv("SLATE_OUTPUT") ? getenv("SLATE_OUTPUT") : OUTPUT_PREFIX);
        return false;
    }
    sui_watch_fd(wl_event_loop_get_fd(server.loop), dispatch, NULL);
    sui_set_poll_hook(flush_clients);
    return true;
}

lv_display_t *sui_backend_open(void)
{
    if (!start()) {
        fprintf(stderr, "slate: couldn't start the compositor\n");
        return NULL;
    }
    return server.base.display;
}

lv_obj_t *sui_layer_system(void)
{
    return server.system.display ? lv_display_get_layer_top(server.system.display) : lv_layer_top();
}
