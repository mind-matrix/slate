#include <stdio.h>
#include <string.h>
#include "net.h"

typedef struct {
    bool enabled;
    bool connected;
    net_join_t join;
} fake_net_t;

static fake_net_t state = { .enabled = true, .connected = true };

static const net_ap_t networks[] = {
    { .ssid = "Home-5G", .path = "/fake/0", .strength = 82, .frequency = 5220, .security = NET_SECURITY_WPA2, .saved = true },
    { .ssid = "Home-2.4G", .path = "/fake/1", .strength = 74, .frequency = 2437, .security = NET_SECURITY_WPA2 },
    { .ssid = "Neighbor-Net", .path = "/fake/2", .strength = 41, .frequency = 5745, .security = NET_SECURITY_WPA3 },
    { .ssid = "Cafe Guest", .path = "/fake/3", .strength = 28, .frequency = 2412, .security = NET_SECURITY_OPEN },
};

bool net_available(void)
{
    return true;
}

bool net_wifi_enabled(void)
{
    return state.enabled;
}

bool net_wifi_set_enabled(bool enabled)
{
    state.enabled = enabled;
    return true;
}

bool net_status(net_status_t *out)
{
    memset(out, 0, sizeof(*out));
    if (!state.enabled)
        return true;
    out->link = state.connected ? NET_LINK_CONNECTED : NET_LINK_DISCONNECTED;
    if (state.connected) {
        snprintf(out->ssid, sizeof(out->ssid), "%s", networks[0].ssid);
        out->frequency = networks[0].frequency;
        snprintf(out->ip, sizeof(out->ip), "192.168.88.3");
    }
    return true;
}

void net_scan(void)
{
}

size_t net_list(net_ap_t *aps, size_t max)
{
    size_t n = 0;
    for (size_t i = 0; i < sizeof(networks) / sizeof(networks[0]) && n < max && state.enabled; i++)
        aps[n++] = networks[i];
    return net_merge_aps(aps, n);
}

bool net_join(const net_ap_t *ap, const char *password)
{
    (void)ap;
    (void)password;
    state.join = NET_JOIN_DONE;
    return true;
}

net_join_t net_join_state(void)
{
    net_join_t result = state.join;
    state.join = NET_JOIN_IDLE;
    return result;
}

bool net_forget_current(void)
{
    state.connected = false;
    return true;
}
