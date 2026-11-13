#include <stdio.h>
#include "bus.h"
#include "sysinfo.h"
#include "update.h"

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

bool update_start_check(void)
{
    return bus_call_unit("StartUnit", UPDATE_CHECK_UNIT);
}

bool update_start_install(void)
{
    return bus_call_unit("StartUnit", UPDATE_INSTALL_UNIT);
}

bool update_cancel(void)
{
    return bus_call_unit("StopUnit", UPDATE_INSTALL_UNIT);
}

void update_read_app(const char *id, update_status_t *out)
{
    char path[UPDATE_PATH_MAX], text[FILE_BUF] = "";
    snprintf(path, sizeof(path), UPDATE_STATE_DIR "/apps/%s/status", id);
    sysinfo_read_file(path, text, sizeof(text));
    update_parse_status(text, out);
}

bool update_start_app_install(const char *id)
{
    char unit[UPDATE_PATH_MAX];
    snprintf(unit, sizeof(unit), UPDATE_APP_INSTALL_UNIT, id);
    return bus_call_unit("StartUnit", unit);
}
