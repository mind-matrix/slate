#ifndef SLATE_WIFI_JOIN_H
#define SLATE_WIFI_JOIN_H

#include "net.h"

typedef void (*wifi_join_cb_t)(const char *ssid);

void wifi_join_open(const net_ap_t *ap, wifi_join_cb_t on_join);
void wifi_join_close(void);

#endif
