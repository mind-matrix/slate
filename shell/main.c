#include "audio.h"
#include "bluetooth.h"
#include "bt.h"
#include "bus.h"
#include "clock_alerts.h"
#include "components/status_bar.h"
#include "desktop.h"
#include "idle.h"
#include "net.h"
#include "notify.h"
#include "platform/platform.h"
#include "quick_drawer.h"
#include "software_update.h"
#include "theme/theme.h"

#define BUS_POLL_MS 200
#define RADIO_POLL_MS 5000
#define ALERT_POLL_MS 1000
#define LEVELS_RETRY_MS 5000

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
    bluetooth_notices_poll();
}

static void update_alerts(void)
{
    const notify_list_t *items = notify_items();
    sui_status_bar_set_alerts(items->count > 0, notify_list_unseen(items) > 0, clock_alerts_any_alarm_on());
}

static void poll_alerts(lv_timer_t *timer)
{
    (void)timer;
    update_alerts();
}

static void apply_levels(lv_timer_t *timer)
{
    if (audio_apply_levels())
        lv_timer_delete(timer);
}

int main(void)
{
    if (!sui_platform_init())
        return 1;

    sui_theme_init();
    notify_load();
    sui_nav_init(&desktop_screen, NULL);
    sui_status_bar_create(lv_layer_top(), quick_drawer_open);

    sd_bus *bus = bus_system();
    if (bus)
        sui_watch_fd(sd_bus_get_fd(bus), on_bus, NULL);
    lv_timer_create(poll_bus, BUS_POLL_MS, NULL);
    bluetooth_agent_init();
    update_radios(lv_timer_create(update_radios, RADIO_POLL_MS, NULL));

    idle_start();
    software_update_start_auto_check();
    clock_alerts_start();
    notify_on_change(update_alerts);
    poll_alerts(lv_timer_create(poll_alerts, ALERT_POLL_MS, NULL));
    apply_levels(lv_timer_create(apply_levels, LEVELS_RETRY_MS, NULL));
    sui_run();
    return 0;
}
