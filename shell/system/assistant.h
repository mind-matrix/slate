#ifndef SLATE_SYSTEM_ASSISTANT_H
#define SLATE_SYSTEM_ASSISTANT_H

#include <stdbool.h>
#include <stddef.h>

/* The registration states an assistant reports through org.slate.Assistant1.ReportStatus. */
typedef enum {
    ASSISTANT_INITIALIZING,
    ASSISTANT_UNREGISTERED,
    ASSISTANT_REGISTERING,
    ASSISTANT_REGISTERED,
    ASSISTANT_UNREGISTERING,
    ASSISTANT_ERROR,
    ASSISTANT_STATE_COUNT,
} assistant_state_t;

#define ASSISTANT_DETAIL_MAX 128
#define ASSISTANT_REASON_MAX 128
#define ASSISTANT_ACTIVITY_CAPTURING "capturing"
#define ASSISTANT_ACTIVITY_IDLE "idle"

bool assistant_parse_state(const char *name, assistant_state_t *out);
const char *assistant_state_name(assistant_state_t state);
bool assistant_valid_activity(const char *activity);
/* Short text from apps (details, reasons) must be printable UTF-8 without control characters
 * and fit the limit. */
bool assistant_valid_text(const char *text, size_t max);

#endif
