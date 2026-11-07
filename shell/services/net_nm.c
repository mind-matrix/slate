#include <stdio.h>
#include <string.h>
#include "bus.h"
#include "net.h"

#define NM "org.freedesktop.NetworkManager"
#define NM_PATH "/org/freedesktop/NetworkManager"
#define NM_SETTINGS_PATH NM_PATH "/Settings"
#define NM_DEVICE NM ".Device"
#define NM_WIRELESS NM ".Device.Wireless"
#define NM_AP NM ".AccessPoint"
#define NM_ACTIVE NM ".Connection.Active"
#define NM_SETTINGS NM ".Settings"
#define NM_CONNECTION NM ".Settings.Connection"
#define NM_IP4 NM ".IP4Config"
#define WIRELESS_GROUP "802-11-wireless"
#define SECURITY_GROUP "802-11-wireless-security"

#define NM_DEVICE_TYPE_WIFI 2
#define NM_DEVICE_STATE_PREPARE 40
#define NM_DEVICE_STATE_ACTIVATED 100
#define NM_ACTIVE_STATE_ACTIVATED 2
#define MAX_DEVICES 16
#define MAX_SAVED 32

typedef struct {
    bus_path_t active;
    bus_path_t created;
} join_t;

typedef struct {
    char ssid[NET_SSID_MAX];
    bus_path_t path;
} saved_t;

typedef struct {
    net_ap_t *ap;
    uint32_t flags;
    uint32_t wpa;
    uint32_t rsn;
} ap_props_t;

static join_t join;

static size_t call_paths(const char *path, const char *iface, const char *method, bus_path_t *out, size_t max)
{
    sd_bus *bus = bus_system();
    sd_bus_message *reply = NULL;
    size_t count = 0;
    if (bus && sd_bus_call_method(bus, NM, path, iface, method, NULL, &reply, "") >= 0)
        count = bus_read_paths(reply, out, max);
    sd_bus_message_unref(reply);
    return count;
}

static bool call_simple(const char *path, const char *iface, const char *method)
{
    sd_bus *bus = bus_system();
    return bus && sd_bus_call_method(bus, NM, path, iface, method, NULL, NULL, "") >= 0;
}

static bool wifi_device(bus_path_t out)
{
    bus_path_t devices[MAX_DEVICES];
    size_t count = call_paths(NM_PATH, NM, "GetDevices", devices, MAX_DEVICES);
    for (size_t i = 0; i < count; i++) {
        uint32_t type = 0;
        if (bus_get_u32(NM, devices[i], NM_DEVICE, "DeviceType", &type) && type == NM_DEVICE_TYPE_WIFI) {
            memcpy(out, devices[i], BUS_PATH_MAX);
            return true;
        }
    }
    return false;
}

static bool ssid_entry(sd_bus_message *m, const char *key, void *user)
{
    const void *bytes;
    size_t count;
    if (strcmp(key, "ssid") != 0 || !bus_variant_bytes(m, &bytes, &count))
        return false;
    net_format_ssid(bytes, count, user, NET_SSID_MAX);
    return true;
}

static bool connection_ssid(const char *path, char *ssid)
{
    sd_bus *bus = bus_system();
    sd_bus_message *reply = NULL;
    ssid[0] = '\0';
    if (!bus || sd_bus_call_method(bus, NM, path, NM_CONNECTION, "GetSettings", NULL, &reply, "") < 0 ||
        sd_bus_message_enter_container(reply, 'a', "{sa{sv}}") <= 0) {
        sd_bus_message_unref(reply);
        return false;
    }
    while (sd_bus_message_enter_container(reply, 'e', "sa{sv}") > 0) {
        const char *group = NULL;
        sd_bus_message_read_basic(reply, 's', &group);
        if (group && strcmp(group, WIRELESS_GROUP) == 0 && sd_bus_message_enter_container(reply, 'a', "{sv}") > 0) {
            bus_read_dict(reply, ssid_entry, ssid);
            sd_bus_message_exit_container(reply);
        } else
            sd_bus_message_skip(reply, "a{sv}");
        sd_bus_message_exit_container(reply);
    }
    sd_bus_message_unref(reply);
    return ssid[0] != '\0';
}

static size_t load_saved(saved_t *out, size_t max)
{
    bus_path_t paths[MAX_SAVED];
    size_t count = call_paths(NM_SETTINGS_PATH, NM_SETTINGS, "ListConnections", paths, MAX_SAVED);
    size_t n = 0;
    for (size_t i = 0; i < count && n < max; i++)
        if (connection_ssid(paths[i], out[n].ssid))
            memcpy(out[n++].path, paths[i], BUS_PATH_MAX);
    return n;
}

static bool find_saved(const char *ssid, bus_path_t out)
{
    saved_t saved[MAX_SAVED];
    size_t count = load_saved(saved, MAX_SAVED);
    for (size_t i = 0; i < count; i++)
        if (strcmp(saved[i].ssid, ssid) == 0) {
            snprintf(out, BUS_PATH_MAX, "%s", saved[i].path);
            return true;
        }
    return false;
}

static bool ap_entry(sd_bus_message *m, const char *key, void *user)
{
    ap_props_t *props = user;
    const void *bytes;
    size_t count;
    if (strcmp(key, "Ssid") == 0) {
        if (!bus_variant_bytes(m, &bytes, &count))
            return false;
        net_format_ssid(bytes, count, props->ap->ssid, sizeof(props->ap->ssid));
        props->ap->raw_len = (uint8_t)(count < NET_SSID_MAX - 1 ? count : NET_SSID_MAX - 1);
        memcpy(props->ap->raw_ssid, bytes, props->ap->raw_len);
        return true;
    }
    if (strcmp(key, "Strength") == 0)
        return bus_variant_basic(m, 'y', &props->ap->strength);
    if (strcmp(key, "Frequency") == 0)
        return bus_variant_basic(m, 'u', &props->ap->frequency);
    if (strcmp(key, "Flags") == 0)
        return bus_variant_basic(m, 'u', &props->flags);
    if (strcmp(key, "WpaFlags") == 0)
        return bus_variant_basic(m, 'u', &props->wpa);
    if (strcmp(key, "RsnFlags") == 0)
        return bus_variant_basic(m, 'u', &props->rsn);
    return false;
}

static bool read_ap(const char *path, net_ap_t *ap)
{
    memset(ap, 0, sizeof(*ap));
    ap_props_t props = { .ap = ap };
    if (!bus_get_all(NM, path, NM_AP, ap_entry, &props))
        return false;
    snprintf(ap->path, sizeof(ap->path), "%s", path);
    ap->security = net_security_from_flags(props.flags, props.wpa, props.rsn);
    return true;
}

static bool address_entry(sd_bus_message *m, const char *key, void *user)
{
    return strcmp(key, "address") == 0 && bus_variant_text(m, 's', user, NET_IP_MAX);
}

static void read_ip(const char *config, char *out)
{
    sd_bus *bus = bus_system();
    sd_bus_message *reply = NULL;
    if (bus && sd_bus_get_property(bus, NM, config, NM_IP4, "AddressData", NULL, &reply, "aa{sv}") >= 0 &&
        sd_bus_message_enter_container(reply, 'a', "a{sv}") > 0 &&
        sd_bus_message_enter_container(reply, 'a', "{sv}") > 0)
        bus_read_dict(reply, address_entry, out);
    sd_bus_message_unref(reply);
}

bool net_available(void)
{
    bus_path_t device;
    return wifi_device(device);
}

bool net_wifi_enabled(void)
{
    bool enabled = false;
    bus_get_bool(NM, NM_PATH, NM, "WirelessEnabled", &enabled);
    return enabled;
}

bool net_wifi_set_enabled(bool enabled)
{
    return bus_set_bool(NM, NM_PATH, NM, "WirelessEnabled", enabled);
}

bool net_status(net_status_t *out)
{
    bus_path_t device, ap, ip4;
    uint32_t state = 0;

    memset(out, 0, sizeof(*out));
    if (!net_wifi_enabled())
        return true;
    out->link = NET_LINK_DISCONNECTED;
    if (!wifi_device(device))
        return false;

    bus_get_u32(NM, device, NM_DEVICE, "State", &state);
    if (state < NM_DEVICE_STATE_PREPARE || state > NM_DEVICE_STATE_ACTIVATED)
        return true;
    out->link = state == NM_DEVICE_STATE_ACTIVATED ? NET_LINK_CONNECTED : NET_LINK_CONNECTING;

    if (bus_get_text(NM, device, NM_WIRELESS, "ActiveAccessPoint", 'o', ap, sizeof(ap)) &&
        strcmp(ap, BUS_NO_OBJECT) != 0) {
        net_ap_t info;
        if (read_ap(ap, &info)) {
            snprintf(out->ssid, sizeof(out->ssid), "%s", info.ssid);
            out->frequency = info.frequency;
        }
    }
    if (out->link == NET_LINK_CONNECTED && bus_get_text(NM, device, NM_DEVICE, "Ip4Config", 'o', ip4, sizeof(ip4)) &&
        strcmp(ip4, BUS_NO_OBJECT) != 0)
        read_ip(ip4, out->ip);
    return true;
}

void net_scan(void)
{
    sd_bus *bus = bus_system();
    bus_path_t device;
    if (bus && wifi_device(device))
        sd_bus_call_method(bus, NM, device, NM_WIRELESS, "RequestScan", NULL, NULL, "a{sv}", 0);
}

size_t net_list(net_ap_t *aps, size_t max)
{
    bus_path_t device, paths[NET_MAX_APS];
    saved_t saved[MAX_SAVED];
    if (!wifi_device(device))
        return 0;

    size_t count = call_paths(device, NM_WIRELESS, "GetAllAccessPoints", paths, NET_MAX_APS);
    size_t saved_count = load_saved(saved, MAX_SAVED);
    size_t n = 0;
    for (size_t i = 0; i < count && n < max; i++) {
        if (!read_ap(paths[i], &aps[n]))
            continue;
        for (size_t j = 0; j < saved_count; j++)
            if (strcmp(saved[j].ssid, aps[n].ssid) == 0)
                aps[n].saved = true;
        n++;
    }
    return net_merge_aps(aps, n);
}

static int append_wireless(sd_bus_message *m, const net_ap_t *ap)
{
    int r;
    if ((r = sd_bus_message_open_container(m, 'e', "sa{sv}")) < 0 ||
        (r = sd_bus_message_append(m, "s", WIRELESS_GROUP)) < 0 ||
        (r = sd_bus_message_open_container(m, 'a', "{sv}")) < 0 ||
        (r = sd_bus_message_open_container(m, 'e', "sv")) < 0 || (r = sd_bus_message_append(m, "s", "ssid")) < 0 ||
        (r = sd_bus_message_open_container(m, 'v', "ay")) < 0 ||
        (r = sd_bus_message_append_array(m, 'y', ap->raw_ssid, ap->raw_len)) < 0 ||
        (r = sd_bus_message_close_container(m)) < 0 || (r = sd_bus_message_close_container(m)) < 0 ||
        (r = sd_bus_message_close_container(m)) < 0)
        return r;
    return sd_bus_message_close_container(m);
}

static int append_settings(sd_bus_message *m, const net_ap_t *ap, const char *password)
{
    int r;
    if ((r = sd_bus_message_open_container(m, 'a', "{sa{sv}}")) < 0 || (r = append_wireless(m, ap)) < 0)
        return r;
    if (password) {
        const char *key_mgmt = ap->security == NET_SECURITY_WPA3 ? "sae" : "wpa-psk";
        if ((r = sd_bus_message_append(m, "{sa{sv}}", SECURITY_GROUP, 2, "key-mgmt", "s", key_mgmt, "psk", "s",
                                       password)) < 0)
            return r;
    }
    return sd_bus_message_close_container(m);
}

static bool add_and_activate(const char *device, const net_ap_t *ap, const char *password)
{
    sd_bus *bus = bus_system();
    sd_bus_message *call = NULL, *reply = NULL;
    const char *created = NULL, *active = NULL;
    bool ok = bus && sd_bus_message_new_method_call(bus, &call, NM, NM_PATH, NM, "AddAndActivateConnection") >= 0 &&
              append_settings(call, ap, password) >= 0 && sd_bus_message_append(call, "oo", device, ap->path) >= 0 &&
              sd_bus_call(bus, call, 0, NULL, &reply) >= 0 && sd_bus_message_read(reply, "oo", &created, &active) > 0;
    if (ok) {
        snprintf(join.created, sizeof(join.created), "%s", created);
        snprintf(join.active, sizeof(join.active), "%s", active);
    }
    sd_bus_message_unref(call);
    sd_bus_message_unref(reply);
    return ok;
}

static bool activate(const char *connection, const char *device, const char *ap)
{
    sd_bus *bus = bus_system();
    sd_bus_message *reply = NULL;
    bool ok = bus &&
              sd_bus_call_method(bus, NM, NM_PATH, NM, "ActivateConnection", NULL, &reply, "ooo", connection, device,
                                 ap) >= 0 &&
              bus_read_text(reply, 'o', join.active, sizeof(join.active));
    sd_bus_message_unref(reply);
    return ok;
}

bool net_join(const net_ap_t *ap, const char *password)
{
    bus_path_t device, connection;
    memset(&join, 0, sizeof(join));
    if (!wifi_device(device))
        return false;
    if (!password && find_saved(ap->ssid, connection))
        return activate(connection, device, ap->path);
    return add_and_activate(device, ap, password);
}

net_join_t net_join_state(void)
{
    if (!join.active[0])
        return NET_JOIN_IDLE;

    uint32_t state = 0;
    if (bus_get_u32(NM, join.active, NM_ACTIVE, "State", &state) && state <= NM_ACTIVE_STATE_ACTIVATED) {
        if (state < NM_ACTIVE_STATE_ACTIVATED)
            return NET_JOIN_ACTIVE;
        memset(&join, 0, sizeof(join));
        return NET_JOIN_DONE;
    }

    if (join.created[0])
        call_simple(join.created, NM_CONNECTION, "Delete");
    memset(&join, 0, sizeof(join));
    return NET_JOIN_FAILED;
}

bool net_forget_current(void)
{
    bus_path_t device, active, connection;
    return wifi_device(device) && bus_get_text(NM, device, NM_DEVICE, "ActiveConnection", 'o', active, sizeof(active)) &&
           strcmp(active, BUS_NO_OBJECT) != 0 &&
           bus_get_text(NM, active, NM_ACTIVE, "Connection", 'o', connection, sizeof(connection)) &&
           call_simple(connection, NM_CONNECTION, "Delete");
}
