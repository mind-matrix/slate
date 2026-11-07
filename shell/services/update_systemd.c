#include "bus.h"
#include "sysinfo.h"
#include "update.h"

#define SYSTEMD "org.freedesktop.systemd1"
#define SYSTEMD_PATH "/org/freedesktop/systemd1"
#define SYSTEMD_MANAGER SYSTEMD ".Manager"
#define FILE_BUF 4096

void update_read(update_status_t *out)
{
    char text[FILE_BUF] = "";
    sysinfo_read_file(UPDATE_STATE_DIR "/status", text, sizeof(text));
    update_parse_status(text, out);
}

size_t update_read_notes(bool available, update_note_t *notes, size_t max)
{
    char text[FILE_BUF];
    const char *path = available ? UPDATE_STATE_DIR "/notes" : UPDATE_INSTALLED_NOTES;
    return sysinfo_read_file(path, text, sizeof(text)) ? update_parse_notes(text, notes, max) : 0;
}

static bool call_unit(const char *method, const char *unit)
{
    sd_bus *bus = bus_system();
    return bus && sd_bus_call_method(bus, SYSTEMD, SYSTEMD_PATH, SYSTEMD_MANAGER, method, NULL, NULL, "ss", unit,
                                     "replace") >= 0;
}

bool update_start_check(void)
{
    return call_unit("StartUnit", UPDATE_CHECK_UNIT);
}

bool update_start_install(void)
{
    return call_unit("StartUnit", UPDATE_INSTALL_UNIT);
}

bool update_cancel(void)
{
    return call_unit("StopUnit", UPDATE_INSTALL_UNIT);
}
