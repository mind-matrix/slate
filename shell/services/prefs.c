#include "prefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "sysinfo.h"

#define CONFIG_DIR ".config/slate"
#define CONFIG_FILE "settings"
#define PATH_LEN 256

static bool append(char *out, size_t len, size_t *used, const char *text, size_t n)
{
    if (*used + n >= len)
        return false;
    memcpy(out + *used, text, n);
    *used += n;
    out[*used] = '\0';
    return true;
}

bool prefs_update(const char *text, const char *key, const char *value, char *out, size_t len)
{
    size_t key_len = strlen(key), used = 0;
    char entry[256];
    int entry_len = snprintf(entry, sizeof(entry), "%s=%s\n", key, value);
    if (entry_len < 0 || (size_t)entry_len >= sizeof(entry) || len == 0)
        return false;
    out[0] = '\0';

    for (const char *line = text; line && *line;) {
        const char *next = strchr(line, '\n');
        size_t n = next ? (size_t)(next - line) + 1 : strlen(line);
        bool same_key = strncmp(line, key, key_len) == 0 && line[key_len] == '=';
        if (!same_key && !append(out, len, &used, line, n))
            return false;
        if (!same_key && !next && !append(out, len, &used, "\n", 1))
            return false;
        line += n;
    }
    return append(out, len, &used, entry, (size_t)entry_len);
}

static bool config_path(char *path, size_t len, bool create_dir)
{
    const char *home = getenv("HOME");
    if (!home || !*home)
        return false;
    if (create_dir) {
        char dir[PATH_LEN];
        snprintf(dir, sizeof(dir), "%s/.config", home);
        mkdir(dir, 0755);
        snprintf(dir, sizeof(dir), "%s/%s", home, CONFIG_DIR);
        mkdir(dir, 0755);
    }
    return snprintf(path, len, "%s/%s/%s", home, CONFIG_DIR, CONFIG_FILE) < (int)len;
}

int prefs_get_int(const char *key, int fallback)
{
    char path[PATH_LEN], text[PREFS_TEXT_MAX], value[64];
    if (!config_path(path, sizeof(path), false) || !sysinfo_read_file(path, text, sizeof(text)) ||
        !sysinfo_parse_key_value(text, key, value, sizeof(value)))
        return fallback;
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    return end != value ? (int)parsed : fallback;
}

bool prefs_set_int(const char *key, int value)
{
    char path[PATH_LEN], tmp[PATH_LEN + 8], text[PREFS_TEXT_MAX] = "", out[PREFS_TEXT_MAX], number[16];
    if (!config_path(path, sizeof(path), true))
        return false;
    sysinfo_read_file(path, text, sizeof(text));
    snprintf(number, sizeof(number), "%d", value);
    if (!prefs_update(text, key, number, out, sizeof(out)))
        return false;

    snprintf(tmp, sizeof(tmp), "%s.new", path);
    FILE *file = fopen(tmp, "w");
    if (!file)
        return false;
    bool written = fputs(out, file) >= 0;
    written = fflush(file) == 0 && written;
    return fclose(file) == 0 && written && rename(tmp, path) == 0;
}
