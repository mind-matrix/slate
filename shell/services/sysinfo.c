#include "sysinfo.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <inttypes.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>

#define KIB UINT64_C(1024)
#define MIB (KIB * KIB)
#define FILE_BUF 4096
#define NET_BUF 8192

static bool read_field(const char *text, const char *key, uint64_t *out)
{
    size_t key_len = strlen(key);
    for (const char *line = text; line && *line; line = strchr(line, '\n') ? strchr(line, '\n') + 1 : NULL) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == ':') {
            *out = strtoull(line + key_len + 1, NULL, 10);
            return true;
        }
    }
    return false;
}

bool sysinfo_parse_meminfo(const char *text, sysinfo_memory_t *out)
{
    *out = (sysinfo_memory_t){ 0 };
    bool ok = read_field(text, "MemTotal", &out->total_kib) && read_field(text, "MemAvailable", &out->available_kib);
    read_field(text, "Cached", &out->cached_kib);
    read_field(text, "SwapTotal", &out->swap_total_kib);
    read_field(text, "SwapFree", &out->swap_free_kib);
    return ok;
}

bool sysinfo_parse_cpu_times(const char *text, sysinfo_cpu_times_t *out)
{
    uint64_t v[8] = { 0 };
    if (sscanf(text, "cpu %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64,
               &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]) < 4)
        return false;
    out->idle = v[3] + v[4];
    out->total = 0;
    for (int i = 0; i < 8; i++)
        out->total += v[i];
    return true;
}

unsigned sysinfo_cpu_percent(sysinfo_cpu_times_t prev, sysinfo_cpu_times_t now)
{
    if (now.total <= prev.total || now.idle < prev.idle)
        return 0;
    uint64_t total = now.total - prev.total;
    uint64_t idle = now.idle - prev.idle;
    return idle >= total ? 0 : (unsigned)((total - idle) * 100 / total);
}

bool sysinfo_parse_net_dev(const char *text, const char *iface, sysinfo_net_counters_t *out)
{
    size_t len = strlen(iface);
    for (const char *line = text; line && *line; line = strchr(line, '\n') ? strchr(line, '\n') + 1 : NULL) {
        while (*line == ' ')
            line++;
        if (strncmp(line, iface, len) != 0 || line[len] != ':')
            continue;
        uint64_t skip;
        return sscanf(line + len + 1, "%" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64
                      " %" SCNu64 " %" SCNu64 " %" SCNu64,
                      &out->rx_bytes, &skip, &skip, &skip, &skip, &skip, &skip, &skip, &out->tx_bytes) == 9;
    }
    return false;
}

bool sysinfo_parse_key_value(const char *text, const char *key, char *value, size_t len)
{
    size_t key_len = strlen(key);
    for (const char *line = text; line && *line; line = strchr(line, '\n') ? strchr(line, '\n') + 1 : NULL) {
        if (strncmp(line, key, key_len) != 0 || line[key_len] != '=')
            continue;
        const char *start = line + key_len + 1;
        const char *end = strchr(start, '\n');
        size_t n = end ? (size_t)(end - start) : strlen(start);
        if (n >= 2 && start[0] == '"' && start[n - 1] == '"') {
            start++;
            n -= 2;
        }
        if (n >= len)
            n = len - 1;
        memcpy(value, start, n);
        value[n] = '\0';
        return true;
    }
    return false;
}

void sysinfo_format_size(uint64_t kib, char *buf, size_t len)
{
    if (kib == 0)
        snprintf(buf, len, "0 B");
    else if (kib < KIB)
        snprintf(buf, len, "%" PRIu64 " KB", kib);
    else if (kib < MIB)
        snprintf(buf, len, "%" PRIu64 " MB", kib / KIB);
    else
        snprintf(buf, len, "%.1f GB", (double)kib / MIB);
}

void sysinfo_format_rate(uint64_t bytes_per_s, char *buf, size_t len)
{
    if (bytes_per_s < KIB)
        snprintf(buf, len, "%" PRIu64 " B/s", bytes_per_s);
    else if (bytes_per_s < MIB)
        snprintf(buf, len, "%" PRIu64 " KB/s", bytes_per_s / KIB);
    else
        snprintf(buf, len, "%.1f MB/s", (double)bytes_per_s / MIB);
}

void sysinfo_format_uptime(uint64_t seconds, char *buf, size_t len)
{
    uint64_t minutes = seconds / 60;
    uint64_t hours = minutes / 60;
    uint64_t days = hours / 24;
    if (hours == 0)
        snprintf(buf, len, "%" PRIu64 " min", minutes);
    else if (days == 0)
        snprintf(buf, len, "%" PRIu64 " h %" PRIu64 " min", hours, minutes % 60);
    else
        snprintf(buf, len, "%" PRIu64 " d %" PRIu64 " h", days, hours % 24);
}

bool sysinfo_read_file(const char *path, char *buf, size_t len)
{
    FILE *file = fopen(path, "r");
    if (!file)
        return false;
    size_t n = fread(buf, 1, len - 1, file);
    fclose(file);
    buf[n] = '\0';
    return n > 0;
}

static void trim(char *text)
{
    size_t n = strlen(text);
    while (n && (text[n - 1] == '\n' || text[n - 1] == ' ' || text[n - 1] == '\0'))
        text[--n] = '\0';
}

static void copy_or_dash(char *dst, size_t len, const char *src)
{
    const char *text = src && *src ? src : "—";
    size_t n = strnlen(text, len - 1);
    memcpy(dst, text, n);
    dst[n] = '\0';
}

void sysinfo_read_device(sysinfo_device_t *out)
{
    char buf[FILE_BUF];
    struct utsname uts;

    *out = (sysinfo_device_t){ 0 };
    if (gethostname(out->hostname, sizeof(out->hostname)) != 0)
        copy_or_dash(out->hostname, sizeof(out->hostname), NULL);
    copy_or_dash(out->kernel, sizeof(out->kernel), uname(&uts) == 0 ? uts.release : NULL);

    if (sysinfo_read_file("/proc/device-tree/model", buf, sizeof(buf))) {
        trim(buf);
        copy_or_dash(out->model, sizeof(out->model), buf);
    } else {
        copy_or_dash(out->model, sizeof(out->model), NULL);
    }

    if (!sysinfo_read_file("/etc/os-release", buf, sizeof(buf)) ||
        !sysinfo_parse_key_value(buf, "PRETTY_NAME", out->base, sizeof(out->base)))
        copy_or_dash(out->base, sizeof(out->base), NULL);

    bool release = sysinfo_read_file("/etc/slate-release", buf, sizeof(buf));
    if (!release || !sysinfo_parse_key_value(buf, "SLATE_BUILD_TYPE", out->build_type, sizeof(out->build_type)))
        copy_or_dash(out->build_type, sizeof(out->build_type), NULL);
    if (!release || !sysinfo_parse_key_value(buf, "SLATE_VERSION", out->version, sizeof(out->version)))
        copy_or_dash(out->version, sizeof(out->version), NULL);
}

static void read_primary_address(sysinfo_sample_t *out)
{
    struct ifaddrs *list;
    copy_or_dash(out->ip, sizeof(out->ip), NULL);
    out->iface[0] = '\0';
    if (getifaddrs(&list) != 0)
        return;
    for (struct ifaddrs *it = list; it; it = it->ifa_next) {
        if (!it->ifa_addr || it->ifa_addr->sa_family != AF_INET || (it->ifa_flags & IFF_LOOPBACK))
            continue;
        inet_ntop(AF_INET, &((struct sockaddr_in *)it->ifa_addr)->sin_addr, out->ip, sizeof(out->ip));
        snprintf(out->iface, sizeof(out->iface), "%s", it->ifa_name);
        break;
    }
    freeifaddrs(list);
}

void sysinfo_sample(sysinfo_sample_t *out)
{
    char buf[NET_BUF];
    struct statvfs fs;

    *out = (sysinfo_sample_t){ .temp_c = -1 };
    if (sysinfo_read_file("/proc/meminfo", buf, sizeof(buf)))
        sysinfo_parse_meminfo(buf, &out->memory);
    if (sysinfo_read_file("/proc/stat", buf, sizeof(buf)))
        sysinfo_parse_cpu_times(buf, &out->cpu);
    if (sysinfo_read_file("/proc/loadavg", buf, sizeof(buf)))
        out->load1 = strtod(buf, NULL);
    if (sysinfo_read_file("/proc/uptime", buf, sizeof(buf)))
        out->uptime_s = strtoull(buf, NULL, 10);
    if (sysinfo_read_file("/sys/class/thermal/thermal_zone0/temp", buf, sizeof(buf)))
        out->temp_c = strtod(buf, NULL) / 1000.0;
    if (sysinfo_read_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", buf, sizeof(buf)))
        out->freq_mhz = (unsigned)(strtoul(buf, NULL, 10) / 1000);
    out->cpu_count = (unsigned)sysconf(_SC_NPROCESSORS_ONLN);

    if (statvfs("/", &fs) == 0) {
        out->disk_total_kib = (uint64_t)fs.f_blocks * fs.f_frsize / KIB;
        out->disk_free_kib = (uint64_t)fs.f_bavail * fs.f_frsize / KIB;
    }

    read_primary_address(out);
    if (out->iface[0] && sysinfo_read_file("/proc/net/dev", buf, sizeof(buf)))
        sysinfo_parse_net_dev(buf, out->iface, &out->net);
}
