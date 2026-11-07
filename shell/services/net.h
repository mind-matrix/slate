#ifndef SLATE_NET_H
#define SLATE_NET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NET_SSID_MAX 33
#define NET_OBJECT_MAX 128
#define NET_IP_MAX 16
#define NET_MAX_APS 32
#define NET_PASSWORD_MIN 8
#define NET_PASSWORD_MAX 64

#define NET_AP_PRIVACY 0x1
#define NET_KEY_MGMT_PSK 0x100
#define NET_KEY_MGMT_8021X 0x200
#define NET_KEY_MGMT_SAE 0x400
#define NET_STRONG_SIGNAL 60

typedef enum {
    NET_SECURITY_OPEN,
    NET_SECURITY_WEP,
    NET_SECURITY_WPA,
    NET_SECURITY_WPA2,
    NET_SECURITY_WPA3,
    NET_SECURITY_ENTERPRISE,
} net_security_t;

typedef struct {
    char ssid[NET_SSID_MAX];
    uint8_t raw_ssid[NET_SSID_MAX];
    uint8_t raw_len;
    char path[NET_OBJECT_MAX];
    uint8_t strength;
    uint32_t frequency;
    net_security_t security;
    bool saved;
} net_ap_t;

typedef enum {
    NET_LINK_OFF,
    NET_LINK_DISCONNECTED,
    NET_LINK_CONNECTING,
    NET_LINK_CONNECTED,
} net_link_t;

typedef struct {
    net_link_t link;
    char ssid[NET_SSID_MAX];
    uint32_t frequency;
    char ip[NET_IP_MAX];
} net_status_t;

typedef enum {
    NET_JOIN_IDLE,
    NET_JOIN_ACTIVE,
    NET_JOIN_DONE,
    NET_JOIN_FAILED,
} net_join_t;

void net_format_ssid(const uint8_t *bytes, size_t count, char *out, size_t len);
net_security_t net_security_from_flags(uint32_t flags, uint32_t wpa_flags, uint32_t rsn_flags);
const char *net_security_label(net_security_t security);
const char *net_band_label(uint32_t frequency);
bool net_needs_password(const net_ap_t *ap);
bool net_can_join(const net_ap_t *ap);
size_t net_merge_aps(net_ap_t *aps, size_t count);
void net_describe_ap(const net_ap_t *ap, char *out, size_t len);

bool net_available(void);
bool net_wifi_enabled(void);
bool net_wifi_set_enabled(bool enabled);
bool net_status(net_status_t *out);
void net_scan(void);
size_t net_list(net_ap_t *aps, size_t max);
bool net_join(const net_ap_t *ap, const char *password);
net_join_t net_join_state(void);
bool net_forget_current(void);

#endif
