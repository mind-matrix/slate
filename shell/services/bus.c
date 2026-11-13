#include "bus.h"

#include <stdio.h>

static sd_bus *system_bus;

sd_bus *bus_system(void)
{
    if (!system_bus && sd_bus_open_system(&system_bus) < 0)
        system_bus = NULL;
    return system_bus;
}

void bus_process(void)
{
    if (system_bus)
        while (sd_bus_process(system_bus, NULL) > 0)
            ;
}

bool bus_read_text(sd_bus_message *m, char type, char *out, size_t len)
{
    const char *value = NULL;
    return sd_bus_message_read_basic(m, type, &value) > 0 && snprintf(out, len, "%s", value) < (int)len;
}

bool bus_read_bytes(sd_bus_message *m, const void **bytes, size_t *count)
{
    return sd_bus_message_read_array(m, 'y', bytes, count) >= 0;
}

bool bus_get_text(const char *service, const char *path, const char *iface, const char *name, char type, char *out,
                  size_t len)
{
    sd_bus *bus = bus_system();
    sd_bus_message *reply = NULL;
    char signature[2] = { type, '\0' };
    bool ok = bus && sd_bus_get_property(bus, service, path, iface, name, NULL, &reply, signature) >= 0 &&
              bus_read_text(reply, type, out, len);
    sd_bus_message_unref(reply);
    return ok;
}

bool bus_get_u32(const char *service, const char *path, const char *iface, const char *name, uint32_t *out)
{
    sd_bus *bus = bus_system();
    return bus && sd_bus_get_property_trivial(bus, service, path, iface, name, NULL, 'u', out) >= 0;
}

bool bus_get_bool(const char *service, const char *path, const char *iface, const char *name, bool *out)
{
    sd_bus *bus = bus_system();
    int value = 0;
    bool ok = bus && sd_bus_get_property_trivial(bus, service, path, iface, name, NULL, 'b', &value) >= 0;
    *out = ok && value;
    return ok;
}

bool bus_set_bool(const char *service, const char *path, const char *iface, const char *name, bool value)
{
    sd_bus *bus = bus_system();
    return bus && sd_bus_set_property(bus, service, path, iface, name, NULL, "b", (int)value) >= 0;
}

void bus_read_dict(sd_bus_message *m, bus_entry_cb_t cb, void *user)
{
    while (sd_bus_message_enter_container(m, 'e', "sv") > 0) {
        const char *key = NULL;
        if (sd_bus_message_read_basic(m, 's', &key) <= 0 || !cb(m, key, user))
            sd_bus_message_skip(m, "v");
        sd_bus_message_exit_container(m);
    }
}

bool bus_variant_basic(sd_bus_message *m, char type, void *out)
{
    char signature[2] = { type, '\0' };
    if (sd_bus_message_enter_container(m, 'v', signature) <= 0)
        return false;
    bool ok = sd_bus_message_read_basic(m, type, out) > 0;
    sd_bus_message_exit_container(m);
    return ok;
}

bool bus_variant_text(sd_bus_message *m, char type, char *out, size_t len)
{
    char signature[2] = { type, '\0' };
    if (sd_bus_message_enter_container(m, 'v', signature) <= 0)
        return false;
    bool ok = bus_read_text(m, type, out, len);
    sd_bus_message_exit_container(m);
    return ok;
}

bool bus_variant_bytes(sd_bus_message *m, const void **bytes, size_t *count)
{
    if (sd_bus_message_enter_container(m, 'v', "ay") <= 0)
        return false;
    bool ok = bus_read_bytes(m, bytes, count);
    sd_bus_message_exit_container(m);
    return ok;
}

bool bus_get_all(const char *service, const char *path, const char *iface, bus_entry_cb_t cb, void *user)
{
    sd_bus *bus = bus_system();
    sd_bus_message *reply = NULL;
    bool ok = bus &&
              sd_bus_call_method(bus, service, path, "org.freedesktop.DBus.Properties", "GetAll", NULL, &reply, "s",
                                 iface) >= 0 &&
              sd_bus_message_enter_container(reply, 'a', "{sv}") > 0;
    if (ok)
        bus_read_dict(reply, cb, user);
    sd_bus_message_unref(reply);
    return ok;
}

size_t bus_read_paths(sd_bus_message *reply, bus_path_t *out, size_t max)
{
    size_t count = 0;
    if (sd_bus_message_enter_container(reply, 'a', "o") <= 0)
        return 0;
    while (count < max && bus_read_text(reply, 'o', out[count], BUS_PATH_MAX))
        count++;
    return count;
}

bool bus_call_unit(const char *method, const char *unit)
{
    sd_bus *bus = bus_system();
    return bus && sd_bus_call_method(bus, "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
                                     "org.freedesktop.systemd1.Manager", method, NULL, NULL, "ss", unit,
                                     "replace") >= 0;
}
