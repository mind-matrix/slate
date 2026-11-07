#include "net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void net_format_ssid(const uint8_t *bytes, size_t count, char *out, size_t len)
{
    size_t n = 0;
    for (size_t i = 0; i < count && n + 1 < len; i++)
        out[n++] = bytes[i] >= 0x20 && bytes[i] < 0x7f ? (char)bytes[i] : '?';
    out[n] = '\0';
}

net_security_t net_security_from_flags(uint32_t flags, uint32_t wpa_flags, uint32_t rsn_flags)
{
    uint32_t key_mgmt = wpa_flags | rsn_flags;
    if (key_mgmt & NET_KEY_MGMT_8021X)
        return NET_SECURITY_ENTERPRISE;
    if (rsn_flags & NET_KEY_MGMT_SAE)
        return NET_SECURITY_WPA3;
    if (rsn_flags & NET_KEY_MGMT_PSK)
        return NET_SECURITY_WPA2;
    if (wpa_flags & NET_KEY_MGMT_PSK)
        return NET_SECURITY_WPA;
    if (flags & NET_AP_PRIVACY)
        return NET_SECURITY_WEP;
    return NET_SECURITY_OPEN;
}

const char *net_security_label(net_security_t security)
{
    switch (security) {
    case NET_SECURITY_WEP:
        return "WEP";
    case NET_SECURITY_WPA:
        return "WPA";
    case NET_SECURITY_WPA2:
        return "WPA2";
    case NET_SECURITY_WPA3:
        return "WPA3";
    case NET_SECURITY_ENTERPRISE:
        return "Enterprise";
    default:
        return "Open";
    }
}

const char *net_band_label(uint32_t frequency)
{
    if (frequency >= 5925)
        return "6 GHz";
    if (frequency >= 4900)
        return "5 GHz";
    return "2.4 GHz";
}

bool net_needs_password(const net_ap_t *ap)
{
    return !ap->saved && ap->security != NET_SECURITY_OPEN;
}

bool net_can_join(const net_ap_t *ap)
{
    return ap->saved || ap->security == NET_SECURITY_OPEN || ap->security == NET_SECURITY_WPA ||
           ap->security == NET_SECURITY_WPA2 || ap->security == NET_SECURITY_WPA3;
}

static int by_strength(const void *a, const void *b)
{
    const net_ap_t *x = a, *y = b;
    if (x->strength != y->strength)
        return (int)y->strength - (int)x->strength;
    return strcmp(x->ssid, y->ssid);
}

size_t net_merge_aps(net_ap_t *aps, size_t count)
{
    size_t kept = 0;
    for (size_t i = 0; i < count; i++) {
        if (!aps[i].ssid[0])
            continue;
        size_t j = 0;
        while (j < kept && strcmp(aps[j].ssid, aps[i].ssid) != 0)
            j++;
        if (j == kept)
            aps[kept++] = aps[i];
        else if (aps[i].strength > aps[j].strength) {
            bool saved = aps[j].saved || aps[i].saved;
            aps[j] = aps[i];
            aps[j].saved = saved;
        } else
            aps[j].saved = aps[j].saved || aps[i].saved;
    }
    qsort(aps, kept, sizeof(*aps), by_strength);
    return kept;
}

void net_describe_ap(const net_ap_t *ap, char *out, size_t len)
{
    snprintf(out, len, "%s · %s%s", net_band_label(ap->frequency), net_security_label(ap->security),
             ap->saved ? " · Saved" : "");
}
