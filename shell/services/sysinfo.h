#ifndef SLATE_SYSINFO_H
#define SLATE_SYSINFO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SYSINFO_TEXT 64

typedef struct {
    uint64_t total_kib;
    uint64_t available_kib;
    uint64_t cached_kib;
    uint64_t swap_total_kib;
    uint64_t swap_free_kib;
} sysinfo_memory_t;

typedef struct {
    uint64_t idle;
    uint64_t total;
} sysinfo_cpu_times_t;

typedef struct {
    uint64_t rx_bytes;
    uint64_t tx_bytes;
} sysinfo_net_counters_t;

typedef struct {
    char hostname[SYSINFO_TEXT];
    char model[SYSINFO_TEXT];
    char kernel[SYSINFO_TEXT];
    char base[SYSINFO_TEXT];
    char build_type[SYSINFO_TEXT];
    char version[SYSINFO_TEXT];
} sysinfo_device_t;

typedef struct {
    sysinfo_memory_t memory;
    sysinfo_cpu_times_t cpu;
    sysinfo_net_counters_t net;
    char iface[SYSINFO_TEXT];
    char ip[SYSINFO_TEXT];
    double load1;
    double temp_c;
    unsigned freq_mhz;
    unsigned cpu_count;
    uint64_t uptime_s;
    uint64_t disk_total_kib;
    uint64_t disk_free_kib;
} sysinfo_sample_t;

bool sysinfo_parse_meminfo(const char *text, sysinfo_memory_t *out);
bool sysinfo_parse_cpu_times(const char *text, sysinfo_cpu_times_t *out);
unsigned sysinfo_cpu_percent(sysinfo_cpu_times_t prev, sysinfo_cpu_times_t now);
bool sysinfo_parse_net_dev(const char *text, const char *iface, sysinfo_net_counters_t *out);
bool sysinfo_parse_key_value(const char *text, const char *key, char *value, size_t len);

void sysinfo_format_size(uint64_t kib, char *buf, size_t len);
void sysinfo_format_rate(uint64_t bytes_per_s, char *buf, size_t len);
void sysinfo_format_uptime(uint64_t seconds, char *buf, size_t len);

bool sysinfo_read_file(const char *path, char *buf, size_t len);
void sysinfo_read_device(sysinfo_device_t *out);
void sysinfo_sample(sysinfo_sample_t *out);

#endif
