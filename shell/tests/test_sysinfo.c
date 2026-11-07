#include <string.h>

#include "check.h"
#include "sysinfo.h"

static const char meminfo[] =
    "MemTotal:        8255488 kB\n"
    "MemFree:         7894016 kB\n"
    "MemAvailable:    7996720 kB\n"
    "Buffers:           12345 kB\n"
    "Cached:           143360 kB\n"
    "SwapCached:            0 kB\n"
    "SwapTotal:       2097136 kB\n"
    "SwapFree:        2097136 kB\n";

static const char net_dev[] =
    "Inter-|   Receive                                                |  Transmit\n"
    " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
    "    lo:    1000      10    0    0    0     0          0         0     1000      10    0    0    0     0       0          0\n"
    " wlan0: 5242880    4000    0    0    0     0          0         0   524288    3000    0    0    0     0       0          0\n";

static const char os_release[] =
    "PRETTY_NAME=\"Debian GNU/Linux 13 (trixie)\"\n"
    "NAME=\"Debian GNU/Linux\"\n"
    "VERSION_ID=\"13\"\n";

static void test_meminfo(void)
{
    sysinfo_memory_t mem;
    CHECK(sysinfo_parse_meminfo(meminfo, &mem));
    CHECK(mem.total_kib == 8255488);
    CHECK(mem.available_kib == 7996720);
    CHECK(mem.cached_kib == 143360);
    CHECK(mem.swap_total_kib == 2097136);
    CHECK(!sysinfo_parse_meminfo("MemFree: 1 kB\n", &mem));
}

static void test_cpu_percent(void)
{
    sysinfo_cpu_times_t a, b;
    CHECK(sysinfo_parse_cpu_times("cpu  100 0 100 700 100 0 0 0 0 0\ncpu0 1 2 3\n", &a));
    CHECK(a.idle == 800 && a.total == 1000);
    CHECK(sysinfo_parse_cpu_times("cpu  150 0 150 1500 200 0 0 0 0 0\n", &b));
    CHECK(sysinfo_cpu_percent(a, b) == 10);
    CHECK(sysinfo_cpu_percent(b, a) == 0);
    CHECK(!sysinfo_parse_cpu_times("intr 1 2 3\n", &a));
}

static void test_net_dev(void)
{
    sysinfo_net_counters_t net;
    CHECK(sysinfo_parse_net_dev(net_dev, "wlan0", &net));
    CHECK(net.rx_bytes == 5242880 && net.tx_bytes == 524288);
    CHECK(!sysinfo_parse_net_dev(net_dev, "eth0", &net));
    CHECK(!sysinfo_parse_net_dev(net_dev, "wlan", &net));
}

static void test_key_value(void)
{
    char value[64];
    CHECK(sysinfo_parse_key_value(os_release, "PRETTY_NAME", value, sizeof(value)));
    CHECK(strcmp(value, "Debian GNU/Linux 13 (trixie)") == 0);
    CHECK(sysinfo_parse_key_value(os_release, "VERSION_ID", value, sizeof(value)));
    CHECK(strcmp(value, "13") == 0);
    CHECK(sysinfo_parse_key_value("SLATE_BUILD_TYPE=dev\n", "SLATE_BUILD_TYPE", value, sizeof(value)));
    CHECK(strcmp(value, "dev") == 0);
    CHECK(!sysinfo_parse_key_value(os_release, "NAME_ID", value, sizeof(value)));
    CHECK(sysinfo_parse_key_value(os_release, "PRETTY_NAME", value, 7));
    CHECK(strcmp(value, "Debian") == 0);
}

static void test_formatting(void)
{
    char buf[32];
    sysinfo_format_size(0, buf, sizeof(buf));
    CHECK(strcmp(buf, "0 B") == 0);
    sysinfo_format_size(259072, buf, sizeof(buf));
    CHECK(strcmp(buf, "253 MB") == 0);
    sysinfo_format_size(8255488, buf, sizeof(buf));
    CHECK(strcmp(buf, "7.9 GB") == 0);
    sysinfo_format_rate(512, buf, sizeof(buf));
    CHECK(strcmp(buf, "512 B/s") == 0);
    sysinfo_format_rate(12288, buf, sizeof(buf));
    CHECK(strcmp(buf, "12 KB/s") == 0);
    sysinfo_format_uptime(300, buf, sizeof(buf));
    CHECK(strcmp(buf, "5 min") == 0);
    sysinfo_format_uptime(8040, buf, sizeof(buf));
    CHECK(strcmp(buf, "2 h 14 min") == 0);
    sysinfo_format_uptime(266400, buf, sizeof(buf));
    CHECK(strcmp(buf, "3 d 2 h") == 0);
}

int main(void)
{
    test_meminfo();
    test_cpu_percent();
    test_net_dev();
    test_key_value();
    test_formatting();
    return failures ? 1 : 0;
}
