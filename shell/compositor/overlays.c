#include "server.h"

#include <stdlib.h>

typedef struct {
    struct wl_list link;
    struct wlr_layer_surface_v1 *surface;
    struct wlr_scene_layer_surface_v1 *scene;
    bool mapped;
    struct wl_listener commit;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
} overlay_t;

static struct {
    struct wl_list all;
    struct wl_listener new_surface;
} overlays;

#define ARRANGE_FIELDS                                                                                     \
    (WLR_LAYER_SURFACE_V1_STATE_DESIRED_SIZE | WLR_LAYER_SURFACE_V1_STATE_ANCHOR |                         \
     WLR_LAYER_SURFACE_V1_STATE_EXCLUSIVE_ZONE | WLR_LAYER_SURFACE_V1_STATE_MARGIN)

static void arrange(overlay_t *overlay)
{
    struct wlr_box full = { 0, 0, SERVER_W, SERVER_H };
    struct wlr_box usable = full;
    wlr_scene_layer_surface_v1_configure(overlay->scene, &full, &usable);
}

static void on_commit(struct wl_listener *listener, void *data)
{
    (void)data;
    overlay_t *overlay = wl_container_of(listener, overlay, commit);
    if (overlay->surface->initial_commit || (overlay->surface->current.committed & ARRANGE_FIELDS))
        arrange(overlay);
}

static void on_map(struct wl_listener *listener, void *data)
{
    (void)data;
    overlay_t *overlay = wl_container_of(listener, overlay, map);
    overlay->mapped = true;
    wlr_scene_node_raise_to_top(&overlay->scene->tree->node);
}

static void on_unmap(struct wl_listener *listener, void *data)
{
    (void)data;
    overlay_t *overlay = wl_container_of(listener, overlay, unmap);
    overlay->mapped = false;
}

static void on_destroy(struct wl_listener *listener, void *data)
{
    (void)data;
    overlay_t *overlay = wl_container_of(listener, overlay, destroy);
    wl_list_remove(&overlay->link);
    wl_list_remove(&overlay->commit.link);
    wl_list_remove(&overlay->map.link);
    wl_list_remove(&overlay->unmap.link);
    wl_list_remove(&overlay->destroy.link);
    free(overlay);
}

static void on_new_surface(struct wl_listener *listener, void *data)
{
    (void)listener;
    struct wlr_layer_surface_v1 *surface = data;
    if (!server_client_permitted(wl_resource_get_client(surface->surface->resource), "overlay")) {
        wlr_layer_surface_v1_destroy(surface);
        return;
    }
    overlay_t *overlay = calloc(1, sizeof(*overlay));
    if (!overlay) {
        wlr_layer_surface_v1_destroy(surface);
        return;
    }
    /* Whatever layer an app asks for, its surfaces sit in the overlay band: above apps and the
     * shell's screens, below the status bar, drawer and system alerts. */
    surface->output = server.output;
    overlay->surface = surface;
    overlay->scene = wlr_scene_layer_surface_v1_create(server.overlays, surface);
    wl_list_insert(&overlays.all, &overlay->link);
    overlay->commit.notify = on_commit;
    wl_signal_add(&surface->surface->events.commit, &overlay->commit);
    overlay->map.notify = on_map;
    wl_signal_add(&surface->surface->events.map, &overlay->map);
    overlay->unmap.notify = on_unmap;
    wl_signal_add(&surface->surface->events.unmap, &overlay->unmap);
    overlay->destroy.notify = on_destroy;
    wl_signal_add(&surface->events.destroy, &overlay->destroy);
}

void overlays_init(void)
{
    wl_list_init(&overlays.all);
    overlays.new_surface.notify = on_new_surface;
    wl_signal_add(&server.layer_shell->events.new_surface, &overlays.new_surface);
}

bool overlays_any(void)
{
    overlay_t *overlay;
    wl_list_for_each(overlay, &overlays.all, link) if (overlay->mapped) return true;
    return false;
}

struct wlr_surface *overlays_surface_at(double x, double y, double *sx, double *sy)
{
    struct wlr_scene_node *node = wlr_scene_node_at(&server.overlays->node, x, y, sx, sy);
    if (!node || node->type != WLR_SCENE_NODE_BUFFER)
        return NULL;
    struct wlr_scene_surface *surface = wlr_scene_surface_try_from_buffer(wlr_scene_buffer_from_node(node));
    return surface ? surface->surface : NULL;
}

/* A tap outside every overlay closes them all; the app is told through the closed event. */
void overlays_dismiss(void)
{
    overlay_t *overlay, *next;
    wl_list_for_each_safe(overlay, next, &overlays.all, link) if (overlay->mapped)
        wlr_layer_surface_v1_destroy(overlay->surface);
}
