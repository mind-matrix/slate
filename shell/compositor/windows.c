#include "server.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "platform/windows.h"

#define EXPECT_SECONDS 30
#define TOKENS_MAX 8
#define TOKEN_MAX 64

struct window {
    struct wl_list link;
    struct wlr_xdg_toplevel *xdg;
    struct wlr_scene_tree *tree;
    char app[ACCESS_ID_MAX];
    bool mapped;
    struct wl_listener commit;
    struct wl_listener map;
    struct wl_listener unmap;
    struct wl_listener destroy;
};

typedef struct {
    char name[TOKEN_MAX];
    char app[ACCESS_ID_MAX];
} token_t;

static struct {
    struct wl_list all;
    struct wl_listener new_toplevel;
    struct wl_listener request_activate;
    char expected[ACCESS_ID_MAX];
    time_t expected_until;
    token_t tokens[TOKENS_MAX];
    size_t next_token;
    sui_windows_cb_t change_cb;
    void *change_user;
} windows;

static window_t *latest_mapped(const char *app, const window_t *except)
{
    window_t *w;
    wl_list_for_each(w, &windows.all, link) if (w != except && w->mapped && strcmp(w->app, app) == 0) return w;
    return NULL;
}

static void set_front(window_t *w)
{
    if (server.front == w)
        return;
    if (server.front) {
        wlr_scene_node_set_enabled(&server.front->tree->node, false);
        wlr_xdg_toplevel_set_activated(server.front->xdg, false);
    }
    server.front = w;
    if (w) {
        wlr_scene_node_set_enabled(&w->tree->node, true);
        wlr_scene_node_raise_to_top(&w->tree->node);
        wlr_xdg_toplevel_set_activated(w->xdg, true);
    }
    /* The shell's screens are fully covered by an app, so they needn't be composited. */
    wlr_scene_node_set_enabled(&server.base.node->node, w == NULL);
    if (!w)
        lv_obj_invalidate(lv_display_get_screen_active(server.base.display));
    input_focus_changed();
    if (windows.change_cb)
        windows.change_cb(windows.change_user);
}

static bool expected(const char *app)
{
    return windows.expected[0] && time(NULL) <= windows.expected_until && strcmp(windows.expected, app) == 0;
}

static void on_commit(struct wl_listener *listener, void *data)
{
    (void)data;
    window_t *w = wl_container_of(listener, w, commit);
    if (!w->xdg->base->initial_commit)
        return;
    wlr_xdg_toplevel_set_size(w->xdg, SERVER_W, SERVER_H - SERVER_APP_Y);
    wlr_xdg_toplevel_set_maximized(w->xdg, true);
    /* A window that starts over after Back while its app is being opened is asked to show at once. */
    if (expected(w->app))
        wlr_xdg_toplevel_set_activated(w->xdg, true);
}

static void on_map(struct wl_listener *listener, void *data)
{
    (void)data;
    window_t *w = wl_container_of(listener, w, map);
    w->mapped = true;
    /* Move it to the head of the list so the newest window of an app is the one shown. */
    wl_list_remove(&w->link);
    wl_list_insert(&windows.all, &w->link);
    bool front_app = server.front && strcmp(server.front->app, w->app) == 0;
    if (expected(w->app) || front_app) {
        windows.expected[0] = '\0';
        set_front(w);
    }
}

static void on_unmap(struct wl_listener *listener, void *data)
{
    (void)data;
    window_t *w = wl_container_of(listener, w, unmap);
    w->mapped = false;
    if (server.front == w)
        set_front(latest_mapped(w->app, w));
}

static void on_destroy(struct wl_listener *listener, void *data)
{
    (void)data;
    window_t *w = wl_container_of(listener, w, destroy);
    if (server.front == w)
        set_front(latest_mapped(w->app, w));
    wl_list_remove(&w->link);
    wl_list_remove(&w->commit.link);
    wl_list_remove(&w->map.link);
    wl_list_remove(&w->unmap.link);
    wl_list_remove(&w->destroy.link);
    free(w);
}

static void on_new_toplevel(struct wl_listener *listener, void *data)
{
    (void)listener;
    struct wlr_xdg_toplevel *xdg = data;
    struct wl_client *client = xdg->base->client->client;
    window_t *w = calloc(1, sizeof(*w));
    if (!w || !server_client_app(client, w->app, sizeof(w->app)) || !server_client_permitted(client, "gui")) {
        free(w);
        wl_resource_post_error(xdg->resource, 0, "this app may not show windows");
        return;
    }
    w->xdg = xdg;
    w->tree = wlr_scene_xdg_surface_create(server.apps, xdg->base);
    wlr_scene_node_set_position(&w->tree->node, 0, SERVER_APP_Y);
    wlr_scene_node_set_enabled(&w->tree->node, false);
    wl_list_insert(&windows.all, &w->link);
    w->commit.notify = on_commit;
    wl_signal_add(&xdg->base->surface->events.commit, &w->commit);
    w->map.notify = on_map;
    wl_signal_add(&xdg->base->surface->events.map, &w->map);
    w->unmap.notify = on_unmap;
    wl_signal_add(&xdg->base->surface->events.unmap, &w->unmap);
    w->destroy.notify = on_destroy;
    wl_signal_add(&xdg->events.destroy, &w->destroy);
}

/* Only tokens Slate issued bring a window forward, and only for the app they were issued to.
 * Tokens apps create for themselves are ignored. */
static void on_request_activate(struct wl_listener *listener, void *data)
{
    (void)listener;
    const struct wlr_xdg_activation_v1_request_activate_event *event = data;
    struct wlr_xdg_toplevel *xdg = wlr_xdg_toplevel_try_from_wlr_surface(event->surface);
    const char *name = wlr_xdg_activation_token_v1_get_name(event->token);
    char app[ACCESS_ID_MAX];
    if (!xdg || !name || !server_client_app(xdg->base->client->client, app, sizeof(app)))
        return;
    for (size_t i = 0; i < TOKENS_MAX; i++) {
        token_t *token = &windows.tokens[i];
        if (!token->name[0] || strcmp(token->name, name) != 0)
            continue;
        bool match = strcmp(token->app, app) == 0;
        token->name[0] = '\0';
        if (!match)
            return;
        window_t *w;
        wl_list_for_each(w, &windows.all, link) if (w->xdg == xdg)
        {
            if (w->mapped)
                set_front(w);
            else
                sui_windows_expect(app);
            return;
        }
        return;
    }
}

void windows_init(void)
{
    wl_list_init(&windows.all);
    windows.new_toplevel.notify = on_new_toplevel;
    wl_signal_add(&server.xdg->events.new_toplevel, &windows.new_toplevel);
    windows.request_activate.notify = on_request_activate;
    wl_signal_add(&server.activation->events.request_activate, &windows.request_activate);
}

struct wlr_surface *windows_surface_at(double x, double y, double *sx, double *sy)
{
    if (!server.front)
        return NULL;
    struct wlr_scene_node *node = wlr_scene_node_at(&server.front->tree->node, x, y, sx, sy);
    if (!node || node->type != WLR_SCENE_NODE_BUFFER)
        return NULL;
    struct wlr_scene_surface *surface = wlr_scene_surface_try_from_buffer(wlr_scene_buffer_from_node(node));
    return surface ? surface->surface : NULL;
}

struct wlr_surface *windows_front_surface(void)
{
    return server.front ? server.front->xdg->base->surface : NULL;
}

/* An app that closed its window at Back keeps the toplevel unmapped. Opening the app again sends
 * it a configure with the activated state, which is the app's cue to map the window again. */
static bool ask_to_map(const char *app)
{
    window_t *w;
    wl_list_for_each(w, &windows.all, link)
    {
        if (w->mapped || strcmp(w->app, app) != 0 || !w->xdg->base->initialized)
            continue;
        sui_windows_expect(app);
        wlr_xdg_toplevel_set_size(w->xdg, SERVER_W, SERVER_H - SERVER_APP_Y);
        wlr_xdg_toplevel_set_activated(w->xdg, true);
        return true;
    }
    return false;
}

bool sui_windows_show(const char *app)
{
    window_t *w = latest_mapped(app, NULL);
    if (!w)
        return ask_to_map(app);
    set_front(w);
    return true;
}

void sui_windows_expect(const char *app)
{
    snprintf(windows.expected, sizeof(windows.expected), "%s", app);
    windows.expected_until = time(NULL) + EXPECT_SECONDS;
}

const char *sui_windows_front(void)
{
    return server.front ? server.front->app : NULL;
}

void sui_windows_back(void)
{
    if (server.front)
        wlr_xdg_toplevel_send_close(server.front->xdg);
}

void sui_windows_home(void)
{
    set_front(NULL);
}

bool sui_windows_token(const char *app, char *out, size_t len)
{
    struct wlr_xdg_activation_token_v1 *token = wlr_xdg_activation_token_v1_create(server.activation);
    const char *name = token ? wlr_xdg_activation_token_v1_get_name(token) : NULL;
    if (!name || strlen(name) >= TOKEN_MAX || strlen(name) >= len)
        return false;
    token_t *slot = &windows.tokens[windows.next_token++ % TOKENS_MAX];
    snprintf(slot->name, sizeof(slot->name), "%s", name);
    snprintf(slot->app, sizeof(slot->app), "%s", app);
    snprintf(out, len, "%s", name);
    return true;
}

void sui_windows_on_change(sui_windows_cb_t cb, void *user)
{
    windows.change_cb = cb;
    windows.change_user = user;
}
