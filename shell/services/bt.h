#ifndef SLATE_BT_H
#define SLATE_BT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BT_NAME_MAX 64
#define BT_ICON_MAX 32
#define BT_OBJECT_MAX 128
#define BT_MAX_DEVICES 32

typedef struct {
    char path[BT_OBJECT_MAX];
    char name[BT_NAME_MAX];
    char icon[BT_ICON_MAX];
    bool named;
    bool paired;
    bool connected;
    int16_t rssi;
} bt_device_t;

typedef enum {
    BT_IDLE,
    BT_PAIRING,
    BT_CONNECTING,
    BT_DISCONNECTING,
    BT_FAILED,
} bt_activity_t;

typedef void (*bt_confirm_cb_t)(const char *device, uint32_t passkey, void *user);
typedef void (*bt_failed_cb_t)(const char *path);

const char *bt_kind_label(const char *icon);
void bt_format_passkey(uint32_t passkey, char *out, size_t len);
size_t bt_sort_devices(bt_device_t *devices, size_t count);
bool bt_path_has_address(const char *path, const char *address);

bool bt_available(void);
bool bt_powered(void);
bool bt_set_powered(bool on);
bool bt_alias(char *out, size_t len);
void bt_set_discovering(bool on);
size_t bt_list(bt_device_t *devices, size_t max);
bool bt_connected_name(char *out, size_t len);
void bt_pair(const char *path);
void bt_connect(const char *path);
void bt_disconnect(const char *path);
void bt_forget(const char *path);
bt_activity_t bt_activity(const char *path);
void bt_agent_start(bt_confirm_cb_t cb, void *user);
void bt_agent_reply(bool accept);
void bt_on_pair_failed(bt_failed_cb_t cb);

#endif
