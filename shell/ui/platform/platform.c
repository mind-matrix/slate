#include "platform/backend.h"

#include <poll.h>
#include <time.h>

#define MAX_WATCHES 8
#define MAX_WAIT_MS 100

typedef struct {
    sui_fd_cb_t cb;
    void *user;
} watch_t;

static struct pollfd fds[MAX_WATCHES];
static watch_t watches[MAX_WATCHES];
static int watch_count;
static bool running;

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

void sui_run(void)
{
    running = true;
    while (running) {
        uint32_t wait = lv_timer_handler();
        if (wait == LV_NO_TIMER_READY || wait > MAX_WAIT_MS)
            wait = MAX_WAIT_MS;
        if (poll(fds, (nfds_t)watch_count, (int)wait) <= 0)
            continue;
        for (int i = 0; i < watch_count; i++)
            if (fds[i].revents)
                watches[i].cb(fds[i].fd, watches[i].user);
    }
}

void sui_quit(void)
{
    running = false;
}
