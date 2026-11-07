#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include "backlight.h"
#include "sysinfo.h"

#define BACKLIGHT_DIR "/sys/class/backlight"
#define PATH_LEN 256

static bool device_file(const char *name, char *path, size_t len)
{
    DIR *dir = opendir(BACKLIGHT_DIR);
    if (!dir)
        return false;

    bool found = false;
    struct dirent *entry;
    while (!found && (entry = readdir(dir)))
        if (entry->d_name[0] != '.')
            found = snprintf(path, len, "%s/%s/%s", BACKLIGHT_DIR, entry->d_name, name) < (int)len;
    closedir(dir);
    return found;
}

static bool read_int(const char *name, int *value)
{
    char path[PATH_LEN], text[32];
    if (!device_file(name, path, sizeof(path)) || !sysinfo_read_file(path, text, sizeof(text)))
        return false;
    *value = atoi(text);
    return true;
}

static bool write_level(int level)
{
    char path[PATH_LEN];
    if (!device_file("brightness", path, sizeof(path)))
        return false;
    FILE *file = fopen(path, "w");
    if (!file)
        return false;
    bool written = fprintf(file, "%d", level) > 0;
    return fclose(file) == 0 && written;
}

bool backlight_get(int *percent)
{
    int level, max;
    if (!read_int("brightness", &level) || !read_int("max_brightness", &max) || max <= 0)
        return false;
    *percent = backlight_level_to_percent(level, max);
    return true;
}

bool backlight_set(int percent)
{
    int max;
    return read_int("max_brightness", &max) && write_level(backlight_percent_to_level(percent, max));
}

bool backlight_blank(void)
{
    return write_level(0);
}
