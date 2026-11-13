#include "platform/backend.h"

#include <poll.h>
#include <time.h>

#define MAX_WATCHES 24
#define MAX_WAIT_MS 100

typedef struct {
    sui_fd_cb_t cb;
    void *user;
} watch_t;

static struct pollfd fds[MAX_WATCHES];
static watch_t watches[MAX_WATCHES];
static int watch_count;
static bool running;
static sui_key_cb_t key_cb;
static void *key_user;
static sui_wake_cb_t wake_cb;
static sui_hook_t poll_hook;

static uint32_t tick_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void lvgl_init(void)
{
    lv_init();
    lv_tick_set_cb(tick_ms);
}

lv_display_t *sui_platform_init(void)
{
    lvgl_init();
    return sui_backend_open();
}

lv_display_t *sui_headless_init(void)
{
    lvgl_init();
    return sui_headless_open();
}

bool sui_watch_fd(int fd, sui_fd_cb_t cb, void *user)
{
    if (watch_count == MAX_WATCHES)
        return false;
    fds[watch_count] = (struct pollfd){ .fd = fd, .events = POLLIN };
    watches[watch_count] = (watch_t){ cb, user };
    watch_count++;
    return true;
}

void sui_unwatch_fd(int fd)
{
    for (int i = 0; i < watch_count; i++) {
        if (fds[i].fd != fd)
            continue;
        watch_count--;
        fds[i] = fds[watch_count];
        watches[i] = watches[watch_count];
        return;
    }
}

void sui_set_key_handler(sui_key_cb_t cb, void *user)
{
    key_cb = cb;
    key_user = user;
}

void sui_key_dispatch(uint32_t key, uint32_t mods)
{
    lv_display_trigger_activity(NULL);
    if (key_cb)
        key_cb(key, mods, key_user);
}

void sui_set_wake_handler(sui_wake_cb_t cb)
{
    wake_cb = cb;
}

bool sui_wake(void)
{
    return wake_cb && wake_cb();
}

void sui_set_poll_hook(sui_hook_t hook)
{
    poll_hook = hook;
}

static void step(void)
{
    uint32_t wait = lv_timer_handler();
    if (wait == LV_NO_TIMER_READY || wait > MAX_WAIT_MS)
        wait = MAX_WAIT_MS;
    if (poll_hook)
        poll_hook();
    if (poll(fds, (nfds_t)watch_count, (int)wait) <= 0)
        return;
    for (int i = 0; i < watch_count; i++)
        if (fds[i].revents)
            watches[i].cb(fds[i].fd, watches[i].user);
}

void sui_run(void)
{
    running = true;
    while (running)
        step();
}

void sui_run_for(uint32_t ms)
{
    uint32_t start = tick_ms();
    while (tick_ms() - start < ms)
        step();
}

void sui_quit(void)
{
    running = false;
}
