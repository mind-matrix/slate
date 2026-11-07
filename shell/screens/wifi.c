#include "wifi.h"

#include <stdio.h>
#include <string.h>
#include "components/button.h"
#include "components/icon.h"
#include "components/list.h"
#include "components/switch.h"
#include "net.h"
#include "settings_ui.h"
#include "theme/theme.h"
#include "wifi_join.h"

#define REFRESH_MS 3000
#define SCAN_EVERY 5
#define SCANNING_TICKS 2
#define SIGNATURE_MAX 1024
#define TEXT 96
#define LOCK_SIZE 20

typedef struct {
    lv_obj_t *page;
    lv_obj_t *toggle;
    lv_obj_t *connected_section;
    lv_obj_t *connected_list;
    lv_obj_t *available_section;
    lv_obj_t *scanning;
    lv_obj_t *networks;
    lv_obj_t *empty_off;
    lv_obj_t *empty_missing;
    lv_timer_t *timer;
    net_ap_t aps[NET_MAX_APS];
    size_t ap_count;
    char signature[SIGNATURE_MAX];
    char connected_signature[TEXT * 2];
    char joining[NET_SSID_MAX];
    unsigned ticks;
} wifi_view_t;

static wifi_view_t view;

void wifi_summary(char *out, size_t len)
{
    net_status_t status;
    net_status(&status);
    if (status.link == NET_LINK_OFF)
        snprintf(out, len, "Off");
    else if (status.link == NET_LINK_CONNECTED && status.ssid[0])
        snprintf(out, len, "%s", status.ssid);
    else if (status.link == NET_LINK_CONNECTING)
        snprintf(out, len, "Connecting…");
    else
        snprintf(out, len, "Not connected");
}

static void forget(lv_event_t *e)
{
    (void)e;
    net_forget_current();
    view.connected_signature[0] = '\0';
    view.signature[0] = '\0';
}

static void show_connected(const net_status_t *status)
{
    char signature[sizeof(view.connected_signature)], detail[TEXT];
    bool visible = status->link == NET_LINK_CONNECTED || status->link == NET_LINK_CONNECTING;

    if (status->link == NET_LINK_CONNECTED)
        snprintf(detail, sizeof(detail), "Connected · %s%s%s", net_band_label(status->frequency),
                 status->ip[0] ? " · " : "", status->ip);
    else
        snprintf(detail, sizeof(detail), "Connecting…");
    snprintf(signature, sizeof(signature), "%d|%s|%s", visible, status->ssid, detail);
    settings_set_visible(view.connected_section, visible);
    if (strcmp(signature, view.connected_signature) == 0)
        return;
    snprintf(view.connected_signature, sizeof(view.connected_signature), "%s", signature);

    lv_obj_clean(view.connected_list);
    if (!visible)
        return;
    sui_row_t spec = { SUI_SYMBOL(LV_SYMBOL_WIFI), SUI_COLOR_CAT_WIFI, status->ssid[0] ? status->ssid : "Wi-Fi", detail,
                       SUI_TRAILING_NONE };
    lv_obj_t *row = sui_list_row_create(view.connected_list, &spec);
    sui_list_row_set_detail_color(row, status->link == NET_LINK_CONNECTED ? SUI_COLOR_SUCCESS : SUI_COLOR_TEXT_MUTED);
    lv_obj_t *button = sui_button_secondary_create(row, "Forget");
    lv_obj_add_event_cb(button, forget, LV_EVENT_CLICKED, NULL);
}

static void on_joined(const char *ssid)
{
    snprintf(view.joining, sizeof(view.joining), "%s", ssid);
    view.connected_signature[0] = '\0';
}

static void pick_network(lv_event_t *e)
{
    size_t index = (size_t)(uintptr_t)lv_event_get_user_data(e);
    if (index >= view.ap_count)
        return;
    const net_ap_t *ap = &view.aps[index];
    if (net_needs_password(ap)) {
        wifi_join_open(ap, on_joined);
        return;
    }
    if (net_join(ap, NULL))
        on_joined(ap->ssid);
}

static void add_network_row(size_t index, const char *current)
{
    const net_ap_t *ap = &view.aps[index];
    char detail[TEXT];
    bool joinable = net_can_join(ap);
    net_describe_ap(ap, detail, sizeof(detail));
    if (!joinable)
        snprintf(detail + strlen(detail), sizeof(detail) - strlen(detail), " · Not supported");

    uint32_t color = ap->strength >= NET_STRONG_SIGNAL ? SUI_COLOR_CAT_WIFI : SUI_COLOR_TEXT_MUTED;
    sui_row_t spec = { SUI_SYMBOL(LV_SYMBOL_WIFI), color, ap->ssid, detail,
                       joinable ? SUI_TRAILING_CHEVRON : SUI_TRAILING_NONE };
    lv_obj_t *row = sui_list_row_create(view.networks, &spec);
    if (ap->security != NET_SECURITY_OPEN) {
        lv_obj_t *lock = sui_glyph_create(row, SUI_VECTOR(SUI_ICON_LOCK), LOCK_SIZE, SUI_COLOR_TEXT_MUTED);
        if (joinable)
            lv_obj_move_to_index(lock, -2);
    }
    if (joinable && strcmp(ap->ssid, current) != 0)
        lv_obj_add_event_cb(row, pick_network, LV_EVENT_CLICKED, (void *)(uintptr_t)index);
}

static void show_networks(const char *current)
{
    char signature[SIGNATURE_MAX] = "";
    size_t used = 0;
    view.ap_count = net_list(view.aps, NET_MAX_APS);

    size_t shown = 0;
    for (size_t i = 0; i < view.ap_count; i++) {
        if (strcmp(view.aps[i].ssid, current) == 0)
            continue;
        shown++;
        int n = snprintf(signature + used, sizeof(signature) - used, "%s|%d|%d|%d;", view.aps[i].ssid,
                         view.aps[i].strength >= NET_STRONG_SIGNAL, view.aps[i].saved, (int)view.aps[i].security);
        if (n < 0 || (size_t)n >= sizeof(signature) - used)
            break;
        used += (size_t)n;
    }
    if (strcmp(signature, view.signature) == 0)
        return;
    snprintf(view.signature, sizeof(view.signature), "%s", signature);

    lv_obj_clean(view.networks);
    if (!shown) {
        sui_row_t spec = { .title = "No networks found", .trailing = SUI_TRAILING_NONE };
        sui_list_row_create(view.networks, &spec);
        return;
    }
    for (size_t i = 0; i < view.ap_count; i++)
        if (strcmp(view.aps[i].ssid, current) != 0)
            add_network_row(i, current);
}

static void check_join(void)
{
    net_join_t state = net_join_state();
    if (state == NET_JOIN_FAILED && view.joining[0]) {
        char body[TEXT * 2];
        snprintf(body, sizeof(body), "Couldn't join %s. Check the password and try again.", view.joining);
        settings_info_dialog("Not connected", body);
    }
    if (state == NET_JOIN_FAILED || state == NET_JOIN_DONE)
        view.joining[0] = '\0';
}

static void refresh(void)
{
    bool available = net_available();
    net_status_t status;
    net_status(&status);
    bool on = status.link != NET_LINK_OFF;

    sui_switch_set(view.toggle, on);
    settings_set_visible(lv_obj_get_parent(view.toggle), available);
    settings_set_visible(view.empty_missing, !available);
    settings_set_visible(view.empty_off, available && !on);
    settings_set_visible(view.available_section, available && on);
    if (!available || !on) {
        settings_set_visible(view.connected_section, false);
        view.connected_signature[0] = '\0';
        view.signature[0] = '\0';
        return;
    }

    check_join();
    show_connected(&status);
    show_networks(status.link == NET_LINK_CONNECTED ? status.ssid : "");
    lv_label_set_text(view.scanning, view.ticks % SCAN_EVERY < SCANNING_TICKS ? "Scanning…" : "");
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    view.ticks++;
    if (view.ticks % SCAN_EVERY == 0)
        net_scan();
    refresh();
}

static void toggle(lv_event_t *e)
{
    (void)e;
    net_wifi_set_enabled(sui_switch_get(view.toggle));
    refresh();
}

static void create(lv_obj_t *screen, void *args)
{
    (void)args;
    memset(&view, 0, sizeof(view));
    view.page = settings_page_create(screen, "Wi-Fi");
    view.toggle = settings_switch_card_create(view.page, "Wi-Fi", NULL, SUI_COLOR_ACCENT);
    lv_obj_add_event_cb(view.toggle, toggle, LV_EVENT_VALUE_CHANGED, NULL);

    view.connected_section = settings_section_create(view.page, "CONNECTED", NULL);
    view.connected_list = sui_list_create(view.connected_section, LV_PCT(100));
    view.available_section = settings_section_create(view.page, "AVAILABLE NETWORKS", &view.scanning);
    view.networks = sui_list_create(view.available_section, LV_PCT(100));
    view.empty_off = settings_empty_state_create(view.page, SUI_SYMBOL(LV_SYMBOL_WIFI), "Wi-Fi is off",
                                                 "Turn it on to see networks.");
    view.empty_missing = settings_empty_state_create(view.page, SUI_SYMBOL(LV_SYMBOL_WIFI), "No Wi-Fi",
                                                     "This device has no Wi-Fi adapter.");
}

static void on_show(lv_obj_t *screen)
{
    (void)screen;
    net_scan();
    view.ticks = 0;
    refresh();
    view.timer = lv_timer_create(tick, REFRESH_MS, NULL);
}

static void on_hide(lv_obj_t *screen)
{
    (void)screen;
    lv_timer_delete(view.timer);
    view.timer = NULL;
    wifi_join_close();
}

const sui_screen_def_t wifi_screen = {
    .name = "wifi",
    .create = create,
    .on_show = on_show,
    .on_hide = on_hide,
};
