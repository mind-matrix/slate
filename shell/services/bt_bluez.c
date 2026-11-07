#include <stdio.h>
#include <string.h>
#include "bt.h"
#include "bus.h"

#define BLUEZ "org.bluez"
#define BLUEZ_ROOT "/org/bluez"
#define ADAPTER_PATH "/org/bluez/hci0"
#define ADAPTER BLUEZ ".Adapter1"
#define DEVICE BLUEZ ".Device1"
#define AGENT_MANAGER BLUEZ ".AgentManager1"
#define AGENT BLUEZ ".Agent1"
#define AGENT_PATH "/org/slate/agent"
#define AGENT_CAPABILITY "DisplayYesNo"
#define REJECTED BLUEZ ".Error.Rejected"
#define ALREADY_EXISTS BLUEZ ".Error.AlreadyExists"
#define LEGACY_PIN "0000"
#define PAIR_TIMEOUT_US (60ull * 1000 * 1000)

typedef struct {
    bus_path_t path;
    bt_activity_t activity;
    sd_bus_slot *slot;
} job_t;

typedef struct {
    bt_confirm_cb_t cb;
    void *user;
    sd_bus_message *pending;
    sd_bus_slot *object;
} agent_t;

static job_t job;
static agent_t agent;

bool bt_available(void)
{
    bool powered;
    return bus_get_bool(BLUEZ, ADAPTER_PATH, ADAPTER, "Powered", &powered);
}

bool bt_powered(void)
{
    bool powered = false;
    bus_get_bool(BLUEZ, ADAPTER_PATH, ADAPTER, "Powered", &powered);
    return powered;
}

bool bt_set_powered(bool on)
{
    return bus_set_bool(BLUEZ, ADAPTER_PATH, ADAPTER, "Powered", on);
}

bool bt_alias(char *out, size_t len)
{
    return bus_get_text(BLUEZ, ADAPTER_PATH, ADAPTER, "Alias", 's', out, len);
}

void bt_set_discovering(bool on)
{
    sd_bus *bus = bus_system();
    if (!bus)
        return;
    bus_set_bool(BLUEZ, ADAPTER_PATH, ADAPTER, "Discoverable", on);
    sd_bus_call_method(bus, BLUEZ, ADAPTER_PATH, ADAPTER, on ? "StartDiscovery" : "StopDiscovery", NULL, NULL, "");
}

static bool device_entry(sd_bus_message *m, const char *key, void *user)
{
    bt_device_t *device = user;
    int flag = 0;
    if (strcmp(key, "Name") == 0)
        device->named = true;
    if (strcmp(key, "Alias") == 0)
        return bus_variant_text(m, 's', device->name, sizeof(device->name));
    if (strcmp(key, "Icon") == 0)
        return bus_variant_text(m, 's', device->icon, sizeof(device->icon));
    if (strcmp(key, "RSSI") == 0)
        return bus_variant_basic(m, 'n', &device->rssi);
    if (strcmp(key, "Paired") == 0 && bus_variant_basic(m, 'b', &flag)) {
        device->paired = flag;
        return true;
    }
    if (strcmp(key, "Connected") == 0 && bus_variant_basic(m, 'b', &flag)) {
        device->connected = flag;
        return true;
    }
    return false;
}

static bool read_device(sd_bus_message *m, const char *path, bt_device_t *device)
{
    bool found = false;
    while (sd_bus_message_enter_container(m, 'e', "sa{sv}") > 0) {
        const char *iface = NULL;
        sd_bus_message_read_basic(m, 's', &iface);
        if (iface && strcmp(iface, DEVICE) == 0 && sd_bus_message_enter_container(m, 'a', "{sv}") > 0) {
            memset(device, 0, sizeof(*device));
            device->rssi = INT16_MIN;
            snprintf(device->path, sizeof(device->path), "%s", path);
            bus_read_dict(m, device_entry, device);
            sd_bus_message_exit_container(m);
            found = true;
        } else
            sd_bus_message_skip(m, "a{sv}");
        sd_bus_message_exit_container(m);
    }
    return found;
}

size_t bt_list(bt_device_t *devices, size_t max)
{
    sd_bus *bus = bus_system();
    sd_bus_message *reply = NULL;
    size_t count = 0;

    if (!bus || sd_bus_call_method(bus, BLUEZ, "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects", NULL,
                                   &reply, "") < 0 ||
        sd_bus_message_enter_container(reply, 'a', "{oa{sa{sv}}}") <= 0) {
        sd_bus_message_unref(reply);
        return 0;
    }
    while (count < max && sd_bus_message_enter_container(reply, 'e', "oa{sa{sv}}") > 0) {
        const char *path = NULL;
        sd_bus_message_read_basic(reply, 'o', &path);
        if (sd_bus_message_enter_container(reply, 'a', "{sa{sv}}") > 0) {
            if (read_device(reply, path, &devices[count]) && strncmp(path, ADAPTER_PATH "/", strlen(ADAPTER_PATH) + 1) == 0)
                count++;
            sd_bus_message_exit_container(reply);
        }
        sd_bus_message_exit_container(reply);
    }
    sd_bus_message_unref(reply);
    return bt_sort_devices(devices, count);
}

bool bt_connected_name(char *out, size_t len)
{
    bt_device_t devices[BT_MAX_DEVICES];
    size_t count = bt_list(devices, BT_MAX_DEVICES);
    for (size_t i = 0; i < count; i++)
        if (devices[i].connected) {
            snprintf(out, len, "%s", devices[i].name);
            return true;
        }
    return false;
}

static bt_failed_cb_t pair_failed;

void bt_on_pair_failed(bt_failed_cb_t cb)
{
    pair_failed = cb;
}

static void report_pair_failure(void)
{
    if (pair_failed)
        pair_failed(job.path);
}

static void finish_job(bt_activity_t result)
{
    job.slot = sd_bus_slot_unref(job.slot);
    job.activity = result;
}

static int call_device(const char *path, const char *method, sd_bus_message_handler_t handler);

static int on_connected(sd_bus_message *reply, void *user, sd_bus_error *error)
{
    (void)user;
    (void)error;
    finish_job(sd_bus_message_is_method_error(reply, NULL) ? BT_FAILED : BT_IDLE);
    return 0;
}

static int on_paired(sd_bus_message *reply, void *user, sd_bus_error *error)
{
    (void)user;
    (void)error;
    const sd_bus_error *failure = sd_bus_message_get_error(reply);
    job.slot = sd_bus_slot_unref(job.slot);
    if (failure && !sd_bus_error_has_name(failure, ALREADY_EXISTS)) {
        job.activity = BT_FAILED;
        report_pair_failure();
        return 0;
    }
    bus_set_bool(BLUEZ, job.path, DEVICE, "Trusted", true);
    job.activity = BT_CONNECTING;
    if (call_device(job.path, "Connect", on_connected) < 0)
        job.activity = BT_FAILED;
    return 0;
}

static int call_device(const char *path, const char *method, sd_bus_message_handler_t handler)
{
    sd_bus *bus = bus_system();
    sd_bus_message *call = NULL;
    if (!bus)
        return -1;
    int r = sd_bus_message_new_method_call(bus, &call, BLUEZ, path, DEVICE, method);
    if (r >= 0)
        r = sd_bus_call_async(bus, &job.slot, call, handler, NULL, PAIR_TIMEOUT_US);
    sd_bus_message_unref(call);
    return r;
}

static void start_job(const char *path, bt_activity_t activity, const char *method, sd_bus_message_handler_t handler)
{
    job.slot = sd_bus_slot_unref(job.slot);
    snprintf(job.path, sizeof(job.path), "%s", path);
    job.activity = activity;
    if (call_device(path, method, handler) < 0) {
        job.activity = BT_FAILED;
        if (activity == BT_PAIRING)
            report_pair_failure();
    }
}

void bt_pair(const char *path)
{
    bt_set_discovering(false);
    start_job(path, BT_PAIRING, "Pair", on_paired);
}

void bt_connect(const char *path)
{
    start_job(path, BT_CONNECTING, "Connect", on_connected);
}

void bt_disconnect(const char *path)
{
    start_job(path, BT_DISCONNECTING, "Disconnect", on_connected);
}

void bt_forget(const char *path)
{
    sd_bus *bus = bus_system();
    if (strcmp(job.path, path) == 0)
        finish_job(BT_IDLE);
    if (bus)
        sd_bus_call_method(bus, BLUEZ, ADAPTER_PATH, ADAPTER, "RemoveDevice", NULL, NULL, "o", path);
}

bt_activity_t bt_activity(const char *path)
{
    return strcmp(job.path, path) == 0 ? job.activity : BT_IDLE;
}

static int reply_empty(sd_bus_message *m, void *user, sd_bus_error *error)
{
    (void)user;
    (void)error;
    return sd_bus_reply_method_return(m, "");
}

static int reply_pin(sd_bus_message *m, void *user, sd_bus_error *error)
{
    (void)user;
    (void)error;
    return sd_bus_reply_method_return(m, "s", LEGACY_PIN);
}

static int reject(sd_bus_message *m, void *user, sd_bus_error *error)
{
    (void)m;
    (void)user;
    return sd_bus_error_set(error, REJECTED, "Not supported");
}

static int request_confirmation(sd_bus_message *m, void *user, sd_bus_error *error)
{
    (void)user;
    (void)error;
    const char *device = NULL;
    uint32_t passkey = 0;
    if (sd_bus_message_read(m, "ou", &device, &passkey) < 0 || !agent.cb)
        return sd_bus_reply_method_errorf(m, REJECTED, "No display");
    sd_bus_message_unref(agent.pending);
    agent.pending = sd_bus_message_ref(m);
    agent.cb(device, passkey, agent.user);
    return 1;
}

static int cancel(sd_bus_message *m, void *user, sd_bus_error *error)
{
    (void)user;
    (void)error;
    agent.pending = sd_bus_message_unref(agent.pending);
    if (agent.cb)
        agent.cb(NULL, 0, agent.user);
    return sd_bus_reply_method_return(m, "");
}

static const sd_bus_vtable agent_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Release", "", "", reply_empty, 0),
    SD_BUS_METHOD("RequestPinCode", "o", "s", reply_pin, 0),
    SD_BUS_METHOD("DisplayPinCode", "os", "", reply_empty, 0),
    SD_BUS_METHOD("RequestPasskey", "o", "u", reject, 0),
    SD_BUS_METHOD("DisplayPasskey", "ouq", "", reply_empty, 0),
    SD_BUS_METHOD("RequestConfirmation", "ou", "", request_confirmation, 0),
    SD_BUS_METHOD("RequestAuthorization", "o", "", reply_empty, 0),
    SD_BUS_METHOD("AuthorizeService", "os", "", reply_empty, 0),
    SD_BUS_METHOD("Cancel", "", "", cancel, 0),
    SD_BUS_VTABLE_END,
};

void bt_agent_start(bt_confirm_cb_t cb, void *user)
{
    sd_bus *bus = bus_system();
    agent.cb = cb;
    agent.user = user;
    if (!bus || agent.object)
        return;
    if (sd_bus_add_object_vtable(bus, &agent.object, AGENT_PATH, AGENT, agent_vtable, NULL) < 0)
        return;
    sd_bus_call_method(bus, BLUEZ, BLUEZ_ROOT, AGENT_MANAGER, "RegisterAgent", NULL, NULL, "os", AGENT_PATH,
                       AGENT_CAPABILITY);
    sd_bus_call_method(bus, BLUEZ, BLUEZ_ROOT, AGENT_MANAGER, "RequestDefaultAgent", NULL, NULL, "o", AGENT_PATH);
}

void bt_agent_reply(bool accept)
{
    if (!agent.pending)
        return;
    if (accept)
        sd_bus_reply_method_return(agent.pending, "");
    else
        sd_bus_reply_method_errorf(agent.pending, REJECTED, "Rejected by user");
    agent.pending = sd_bus_message_unref(agent.pending);
}
