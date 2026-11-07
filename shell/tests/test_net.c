#include <string.h>

#include "check.h"
#include "net.h"

static void test_format_ssid(void)
{
    char out[NET_SSID_MAX];
    const uint8_t plain[] = { 'H', 'o', 'm', 'e', '-', '5', 'G' };
    net_format_ssid(plain, sizeof(plain), out, sizeof(out));
    CHECK(strcmp(out, "Home-5G") == 0);

    const uint8_t odd[] = { 'A', 0x00, 0xC3, 'B' };
    net_format_ssid(odd, sizeof(odd), out, sizeof(out));
    CHECK(strcmp(out, "A??B") == 0);

    net_format_ssid(plain, sizeof(plain), out, 4);
    CHECK(strcmp(out, "Hom") == 0);

    net_format_ssid(plain, 0, out, sizeof(out));
    CHECK(out[0] == '\0');
}

static void test_security(void)
{
    CHECK(net_security_from_flags(0, 0, 0) == NET_SECURITY_OPEN);
    CHECK(net_security_from_flags(NET_AP_PRIVACY, 0, 0) == NET_SECURITY_WEP);
    CHECK(net_security_from_flags(NET_AP_PRIVACY, NET_KEY_MGMT_PSK, 0) == NET_SECURITY_WPA);
    CHECK(net_security_from_flags(NET_AP_PRIVACY, 0, NET_KEY_MGMT_PSK) == NET_SECURITY_WPA2);
    CHECK(net_security_from_flags(NET_AP_PRIVACY, 0, NET_KEY_MGMT_PSK | NET_KEY_MGMT_SAE) == NET_SECURITY_WPA3);
    CHECK(net_security_from_flags(NET_AP_PRIVACY, 0, NET_KEY_MGMT_8021X) == NET_SECURITY_ENTERPRISE);
    CHECK(strcmp(net_security_label(NET_SECURITY_WPA2), "WPA2") == 0);
    CHECK(strcmp(net_security_label(NET_SECURITY_OPEN), "Open") == 0);
}

static void test_band(void)
{
    CHECK(strcmp(net_band_label(2437), "2.4 GHz") == 0);
    CHECK(strcmp(net_band_label(5220), "5 GHz") == 0);
    CHECK(strcmp(net_band_label(5955), "6 GHz") == 0);
}

static void test_join_rules(void)
{
    net_ap_t ap = { .security = NET_SECURITY_WPA2 };
    CHECK(net_needs_password(&ap));
    CHECK(net_can_join(&ap));
    ap.saved = true;
    CHECK(!net_needs_password(&ap));
    ap = (net_ap_t){ .security = NET_SECURITY_OPEN };
    CHECK(!net_needs_password(&ap));
    ap = (net_ap_t){ .security = NET_SECURITY_ENTERPRISE };
    CHECK(!net_can_join(&ap));
    ap.saved = true;
    CHECK(net_can_join(&ap));
}

static void test_merge(void)
{
    net_ap_t aps[] = {
        { .ssid = "Home", .strength = 40 },
        { .ssid = "", .strength = 90 },
        { .ssid = "Cafe", .strength = 60 },
        { .ssid = "Home", .strength = 80 },
        { .ssid = "Home", .strength = 20, .saved = true },
    };
    size_t count = net_merge_aps(aps, 5);
    CHECK(count == 2);
    CHECK(strcmp(aps[0].ssid, "Home") == 0 && aps[0].strength == 80 && aps[0].saved);
    CHECK(strcmp(aps[1].ssid, "Cafe") == 0);
}

static void test_describe(void)
{
    char out[64];
    net_ap_t ap = { .frequency = 2412, .security = NET_SECURITY_WPA2, .saved = true };
    net_describe_ap(&ap, out, sizeof(out));
    CHECK(strcmp(out, "2.4 GHz · WPA2 · Saved") == 0);
}

int main(void)
{
    test_format_ssid();
    test_security();
    test_band();
    test_join_rules();
    test_merge();
    test_describe();
    return failures ? 1 : 0;
}
