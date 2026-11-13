#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "apps.h"
#include "bus.h"
#include "sysinfo.h"

#define FILE_BUF 2048
#define SUFFIX ".app"

static void read_version(const char *id, char *out, size_t len)
{
    char command[96];
    snprintf(command, sizeof(command), "dpkg-query -W -f='${Version}' slate-app-%s 2>/dev/null", id);
    FILE *pipe = popen(command, "r");
    out[0] = '\0';
    if (!pipe)
        return;
    if (!fgets(out, (int)len, pipe))
        out[0] = '\0';
    pclose(pipe);
}

static bool load(const char *id, const char *dev_apps, app_t *out)
{
    char path[APPS_PATH_MAX], text[FILE_BUF];
    snprintf(path, sizeof(path), "%s/%s.app", APPS_DIR, id);
    if (!sysinfo_read_file(path, text, sizeof(text)))
        return false;
    apps_parse_app(text, id, out);
    snprintf(path, sizeof(path), "%s/%s.permissions", APPS_GRANTED_DIR, id);
    if (sysinfo_read_file(path, text, sizeof(text)))
        snprintf(out->granted, sizeof(out->granted), "%.*s", (int)strcspn(text, "\n"), text);
    snprintf(path, sizeof(path), "%s/%s.png", APPS_DIR, id);
    if (access(path, R_OK) == 0)
        snprintf(out->logo, sizeof(out->logo), "%s", path);
    read_version(id, out->version, sizeof(out->version));
    out->dev = apps_has(dev_apps, id);
    return true;
}

size_t apps_list(app_t *out, size_t max)
{
    char image[FILE_BUF] = "", dev_apps[APPS_PERMISSIONS_MAX] = "";
    sysinfo_read_file(APPS_IMAGE_FILE, image, sizeof(image));
    sysinfo_parse_key_value(image, "SLATE_DEV_APPS", dev_apps, sizeof(dev_apps));

    DIR *dir = opendir(APPS_DIR);
    if (!dir)
        return 0;
    size_t count = 0;
    struct dirent *entry;
    while (count < max && (entry = readdir(dir))) {
        size_t n = strlen(entry->d_name);
        if (n <= strlen(SUFFIX) || strcmp(entry->d_name + n - strlen(SUFFIX), SUFFIX) != 0)
            continue;
        char id[APPS_ID_MAX];
        snprintf(id, sizeof(id), "%.*s", (int)(n - strlen(SUFFIX)), entry->d_name);
        if (apps_valid_id(id) && load(id, dev_apps, &out[count]))
            count++;
    }
    closedir(dir);
    apps_sort(out, count);
    return count;
}

bool apps_permission_text(const char *permission, char *title, size_t title_len, char *detail, size_t detail_len)
{
    char path[APPS_PATH_MAX], text[FILE_BUF];
    snprintf(path, sizeof(path), "%s/%s.conf", APPS_PERMISSIONS_DIR, permission);
    if (!sysinfo_read_file(path, text, sizeof(text)) || !sysinfo_parse_key_value(text, "title", title, title_len))
        return false;
    if (!sysinfo_parse_key_value(text, "detail", detail, detail_len))
        detail[0] = '\0';
    return true;
}

static bool call(const char *method, const char *id)
{
    char unit[APPS_ID_MAX + 24];
    if (!apps_valid_id(id))
        return false;
    snprintf(unit, sizeof(unit), "slate-app@%s.service", id);
    return bus_call_unit(method, unit);
}

bool apps_start(const char *id)
{
    return call("StartUnit", id);
}

bool apps_restart(const char *id)
{
    return call("RestartUnit", id);
}

bool apps_running(const char *id)
{
    char unit[APPS_ID_MAX + 24], path[BUS_PATH_MAX], state[32];
    sd_bus *bus = bus_system();
    sd_bus_message *reply = NULL;
    const char *object = NULL;
    if (!bus || !apps_valid_id(id))
        return false;
    snprintf(unit, sizeof(unit), "slate-app@%s.service", id);
    if (sd_bus_call_method(bus, "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
                           "org.freedesktop.systemd1.Manager", "GetUnit", NULL, &reply, "s", unit) < 0)
        return false;
    bool found = sd_bus_message_read(reply, "o", &object) >= 0 && object;
    if (found)
        snprintf(path, sizeof(path), "%s", object);
    sd_bus_message_unref(reply);
    return found && bus_get_text("org.freedesktop.systemd1", path, "org.freedesktop.systemd1.Unit", "ActiveState", 's',
                                 state, sizeof(state)) &&
           strcmp(state, "active") == 0;
}
