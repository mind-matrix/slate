#include "nav/nav.h"

#include "platform/windows.h"
#include "theme/theme.h"

#define EDGE_SWIPE_W 24

typedef struct {
    const sui_screen_def_t *def;
    lv_obj_t *screen;
} entry_t;

static entry_t stack[SUI_NAV_MAX];
static size_t depth;

static void notify(void (*cb)(lv_obj_t *), lv_obj_t *screen)
{
    if (cb)
        cb(screen);
}

static void edge_gesture(lv_event_t *e)
{
    (void)e;
    if (lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_RIGHT)
        sui_nav_pop();
}

static void add_edge_swipe(lv_obj_t *screen)
{
    lv_obj_t *edge = lv_obj_create(screen);
    lv_obj_remove_flag(edge, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_size(edge, EDGE_SWIPE_W, LV_PCT(100));
    lv_obj_add_event_cb(edge, edge_gesture, LV_EVENT_GESTURE, NULL);
}

static void show(entry_t *entry)
{
    lv_screen_load(entry->screen);
    notify(entry->def->on_show, entry->screen);
}

void sui_nav_init(const sui_screen_def_t *home, void *args)
{
    depth = 0;
    sui_nav_push(home, args);
}

/* The shell's screens sit below app windows and overlays, so showing one sends the app in front to the
 * background, as Home does, and closes overlays, as a tap outside them does. Otherwise a screen opened from
 * the drawer would open behind them. */
static void bring_shell_forward(void)
{
    if (sui_windows_front())
        sui_windows_home();
    sui_windows_dismiss_overlays();
}

void sui_nav_push(const sui_screen_def_t *def, void *args)
{
    bring_shell_forward();
    if (depth == SUI_NAV_MAX)
        return;
    if (depth)
        notify(stack[depth - 1].def->on_hide, stack[depth - 1].screen);

    lv_obj_t *screen = sui_screen_create();
    def->create(screen, args);
    if (depth)
        add_edge_swipe(screen);

    stack[depth] = (entry_t){ def, screen };
    show(&stack[depth++]);
}

void sui_nav_pop(void)
{
    if (depth < 2)
        return;
    entry_t top = stack[--depth];
    notify(top.def->on_hide, top.screen);
    show(&stack[depth - 1]);
    lv_obj_delete_async(top.screen);
}

void sui_nav_home(void)
{
    bring_shell_forward();
    if (depth < 2)
        return;
    notify(stack[depth - 1].def->on_hide, stack[depth - 1].screen);
    while (depth > 1)
        lv_obj_delete_async(stack[--depth].screen);
    show(&stack[0]);
}

size_t sui_nav_depth(void)
{
    return depth;
}

const sui_screen_def_t *sui_nav_top(void)
{
    return depth ? stack[depth - 1].def : NULL;
}
