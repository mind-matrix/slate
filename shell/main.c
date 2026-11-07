#include "bluetooth.h"
#include "bt.h"
#include "bus.h"
#include "components/status_bar.h"
#include "desktop.h"
#include "idle.h"
#include "net.h"
#include "platform/platform.h"
#include "quick_drawer.h"
#include "software_update.h"
#include "theme/theme.h"

#define BUS_POLL_MS 200
#define RADIO_POLL_MS 5000

static void on_bus(int fd, void *user)
{
    (void)fd;
    (void)user;
    bus_process();
}

static void poll_bus(lv_timer_t *timer)
{
    (void)timer;
    bus_process();
}

static void update_radios(lv_timer_t *timer)
{
    (void)timer;
    sui_status_bar_set_radios(net_wifi_enabled(), bt_powered());
}

int main(void)
{
    if (!sui_platform_init())
        return 1;

    sui_theme_init();
    sui_nav_init(&desktop_screen, NULL);
    sui_status_bar_create(lv_layer_top(), quick_drawer_open);

    sd_bus *bus = bus_system();
    if (bus)
        sui_watch_fd(sd_bus_get_fd(bus), on_bus, NULL);
    lv_timer_create(poll_bus, BUS_POLL_MS, NULL);
    update_radios(lv_timer_create(update_radios, RADIO_POLL_MS, NULL));

    bluetooth_agent_init();
    idle_start();
    software_update_start_auto_check();
    sui_run();
    return 0;
}
