#include "timezone.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>
#include <time.h>
#include <unistd.h>

#define ZONE_TABLE "/usr/share/zoneinfo/zone1970.tab"
#define LOCALTIME_LINK "/etc/localtime"
#define ZONEINFO_MARKER "zoneinfo/"
#define TABLE_BUF (64 * 1024)
#define TIMEDATE_SERVICE "org.freedesktop.timedate1"
#define TIMEDATE_PATH "/org/freedesktop/timedate1"

static int compare_ids(const void *a, const void *b)
{
    return strcmp(a, b);
}

static void copy_field(char *dst, size_t len, const char *start, size_t n)
{
    if (n >= len)
        n = len - 1;
    memcpy(dst, start, n);
    dst[n] = '\0';
}

size_t timezone_parse_table(const char *text, timezone_id_t *ids, size_t max)
{
    size_t count = 0;
    if (count < max)
        snprintf(ids[count++], TIMEZONE_ID_MAX, "%s", TIMEZONE_UTC);

    for (const char *line = text; line && *line && count < max; line = strchr(line, '\n') ? strchr(line, '\n') + 1 : NULL) {
        if (*line == '#' || *line == '\n')
            continue;
        const char *zone = line;
        for (int tabs = 0; tabs < 2 && zone; tabs++)
            zone = strchr(zone, '\t') ? strchr(zone, '\t') + 1 : NULL;
        if (!zone)
            continue;
        size_t n = strcspn(zone, "\t\n");
        if (n == 0)
            continue;
        copy_field(ids[count++], TIMEZONE_ID_MAX, zone, n);
    }
    qsort(ids + 1, count - 1, sizeof(timezone_id_t), compare_ids);
    return count;
}

bool timezone_region(const char *id, char *region, size_t len)
{
    const char *slash = strchr(id, '/');
    if (!slash || strncmp(id, "Etc/", 4) == 0) {
        snprintf(region, len, "UTC");
        return false;
    }
    copy_field(region, len, id, (size_t)(slash - id));
    return true;
}

void timezone_city(const char *id, char *city, size_t len)
{
    if (strncmp(id, "Etc/", 4) == 0 || !strchr(id, '/')) {
        snprintf(city, len, "UTC");
        return;
    }
    snprintf(city, len, "%s", strrchr(id, '/') + 1);
    for (char *c = city; *c; c++)
        if (*c == '_')
            *c = ' ';
}

void timezone_format_offset(long seconds, char *buf, size_t len)
{
    const char *sign = seconds < 0 ? "\xE2\x88\x92" : "+";
    long magnitude = labs(seconds) / 60;
    snprintf(buf, len, "UTC%s%02ld:%02ld", sign, magnitude / 60, magnitude % 60);
}

bool timezone_from_link(const char *target, char *id, size_t len)
{
    const char *marker = strstr(target, ZONEINFO_MARKER);
    if (!marker || !marker[strlen(ZONEINFO_MARKER)])
        return false;
    snprintf(id, len, "%s", marker + strlen(ZONEINFO_MARKER));
    return true;
}

size_t timezone_load_all(timezone_id_t *ids, size_t max)
{
    char *text = malloc(TABLE_BUF);
    FILE *file = text ? fopen(ZONE_TABLE, "r") : NULL;
    size_t count = 0;

    if (file) {
        size_t n = fread(text, 1, TABLE_BUF - 1, file);
        text[n] = '\0';
        fclose(file);
        count = timezone_parse_table(text, ids, max);
    } else if (max) {
        snprintf(ids[0], TIMEZONE_ID_MAX, "%s", TIMEZONE_UTC);
        count = 1;
    }
    free(text);
    return count;
}

bool timezone_current(char *id, size_t len)
{
    char target[256];
    ssize_t n = readlink(LOCALTIME_LINK, target, sizeof(target) - 1);
    if (n > 0) {
        target[n] = '\0';
        if (timezone_from_link(target, id, len))
            return true;
    }
    snprintf(id, len, "%s", TIMEZONE_UTC);
    return false;
}

long timezone_offset(const char *id)
{
    const char *saved = getenv("TZ");
    char previous[TIMEZONE_ID_MAX + 1] = { 0 };
    char zone[TIMEZONE_ID_MAX + 1];
    struct tm local;
    time_t now = time(NULL);

    if (saved)
        snprintf(previous, sizeof(previous), "%s", saved);
    snprintf(zone, sizeof(zone), ":%s", id);
    setenv("TZ", zone, 1);
    tzset();
    localtime_r(&now, &local);

    if (saved)
        setenv("TZ", previous, 1);
    else
        unsetenv("TZ");
    tzset();
    return local.tm_gmtoff;
}

bool timezone_set(const char *id)
{
    sd_bus *bus = NULL;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    bool ok = sd_bus_open_system(&bus) >= 0 &&
              sd_bus_call_method(bus, TIMEDATE_SERVICE, TIMEDATE_PATH, TIMEDATE_SERVICE, "SetTimezone", &error, NULL,
                                 "sb", id, 0) >= 0;
    if (!ok)
        fprintf(stderr, "slate: set timezone %s failed: %s\n", id, error.message ? error.message : "no bus");
    sd_bus_error_free(&error);
    sd_bus_unref(bus);
    tzset();
    return ok;
}

bool timezone_ntp_synced(void)
{
    sd_bus *bus = NULL;
    int synced = 0;
    if (sd_bus_open_system(&bus) >= 0)
        sd_bus_get_property_trivial(bus, TIMEDATE_SERVICE, TIMEDATE_PATH, TIMEDATE_SERVICE, "NTPSynchronized", NULL,
                                    'b', &synced);
    sd_bus_unref(bus);
    return synced;
}
