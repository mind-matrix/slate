#ifndef SLATE_UPDATE_H
#define SLATE_UPDATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UPDATE_TEXT_MAX 32
#define UPDATE_NOTE_MAX 160
#define UPDATE_MAX_NOTES 8
#define UPDATE_STATE_DIR "/var/lib/slate-update"
#define UPDATE_INSTALLED_NOTES "/usr/share/slate/notes"
#define UPDATE_CHECK_UNIT "slate-update-check.service"
#define UPDATE_INSTALL_UNIT "slate-update-install.service"
#define UPDATE_DAY_SECONDS 86400

typedef enum {
    UPDATE_UNKNOWN,
    UPDATE_DISABLED,
    UPDATE_CURRENT,
    UPDATE_CHECKING,
    UPDATE_AVAILABLE,
    UPDATE_DOWNLOADING,
    UPDATE_INSTALLING,
    UPDATE_RESTART,
    UPDATE_ERROR,
} update_state_t;

typedef struct {
    update_state_t state;
    char version[UPDATE_TEXT_MAX];
    char reason[UPDATE_TEXT_MAX];
    uint64_t size;
    int progress;
    int64_t checked;
} update_status_t;

typedef char update_note_t[UPDATE_NOTE_MAX];

void update_parse_status(const char *text, update_status_t *out);
size_t update_parse_notes(const char *text, update_note_t *notes, size_t max);
bool update_is_busy(update_state_t state);
bool update_check_due(const update_status_t *status, int64_t now);
void update_format_size(uint64_t bytes, char *out, size_t len);

void update_read(update_status_t *out);
size_t update_read_notes(bool available, update_note_t *notes, size_t max);
bool update_start_check(void);
bool update_start_install(void);
bool update_cancel(void);

#endif
