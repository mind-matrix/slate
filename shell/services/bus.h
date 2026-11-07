#ifndef SLATE_BUS_H
#define SLATE_BUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <systemd/sd-bus.h>

#define BUS_PATH_MAX 128
#define BUS_NO_OBJECT "/"

typedef char bus_path_t[BUS_PATH_MAX];

sd_bus *bus_system(void);
void bus_process(void);
bool bus_get_text(const char *service, const char *path, const char *iface, const char *name, char type, char *out,
                  size_t len);
bool bus_get_u32(const char *service, const char *path, const char *iface, const char *name, uint32_t *out);
bool bus_get_bool(const char *service, const char *path, const char *iface, const char *name, bool *out);
bool bus_set_bool(const char *service, const char *path, const char *iface, const char *name, bool value);
size_t bus_read_paths(sd_bus_message *reply, bus_path_t *out, size_t max);
bool bus_read_text(sd_bus_message *m, char type, char *out, size_t len);
bool bus_read_bytes(sd_bus_message *m, const void **bytes, size_t *count);

typedef bool (*bus_entry_cb_t)(sd_bus_message *m, const char *key, void *user);

void bus_read_dict(sd_bus_message *m, bus_entry_cb_t cb, void *user);
bool bus_variant_basic(sd_bus_message *m, char type, void *out);
bool bus_variant_text(sd_bus_message *m, char type, char *out, size_t len);
bool bus_variant_bytes(sd_bus_message *m, const void **bytes, size_t *count);
bool bus_get_all(const char *service, const char *path, const char *iface, bus_entry_cb_t cb, void *user);

#endif
