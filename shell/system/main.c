/* slate-system: Slate's key store and voice assistant selection, on the system bus as
 * org.slate.System. Callers are identified by the user they run as, from the bus. */
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include "access.h"
#include "assistant.h"
#include "keystore.h"
#include "vault.h"

#define BUS_NAME "org.slate.System"
#define KEYSTORE_PATH "/org/slate/Keystore"
#define KEYSTORE_IFACE "org.slate.Keystore1"
#define ASSISTANT_PATH "/org/slate/Assistant"
#define ASSISTANT_IFACE "org.slate.Assistant1"
#define ERROR_NOT_FOUND "org.slate.Keystore1.Error.NotFound"
#define ERROR_FULL "org.slate.Keystore1.Error.Full"
#define DEFAULT_STATE_DIR "/var/lib/slate-system"
#define DEFAULT_PERMISSIONS_DIR ACCESS_PERMISSIONS_DIR
#define STATUS_MAX 16
#define ACTIVITY_MAX 16
#define USER_MAX 64
#define PATH_LEN 512
#define PERMISSIONS_BUF 512

typedef struct {
    char user[USER_MAX];
    bool shell;
    char app[ACCESS_ID_MAX];
} caller_t;

typedef struct {
    char id[ACCESS_ID_MAX];
    assistant_state_t state;
    char detail[ASSISTANT_DETAIL_MAX];
    char activity[ACTIVITY_MAX];
} status_t;

static struct {
    sd_bus *bus;
    const char *state_dir;
    const char *permissions_dir;
    char active[ACCESS_ID_MAX];
    status_t statuses[STATUS_MAX];
} sys;

/* ---- callers ---------------------------------------------------------------------------- */

static int caller_of(sd_bus_message *m, caller_t *caller)
{
    sd_bus_creds *creds = NULL;
    uid_t uid;
    int r = sd_bus_query_sender_creds(m, SD_BUS_CREDS_EUID | SD_BUS_CREDS_UID, &creds);
    if (r >= 0) {
        r = sd_bus_creds_get_euid(creds, &uid);
        if (r < 0)
            r = sd_bus_creds_get_uid(creds, &uid);
    }
    sd_bus_creds_unref(creds);
    if (r < 0)
        return r;
    struct passwd pw, *found = NULL;
    char buf[1024];
    if (getpwuid_r(uid, &pw, buf, sizeof(buf), &found) != 0 || !found)
        return -ESRCH;
    snprintf(caller->user, sizeof(caller->user), "%s", found->pw_name);
    caller->shell = strcmp(found->pw_name, KEYSTORE_SHELL_USER) == 0;
    if (!access_app_from_user(found->pw_name, caller->app, sizeof(caller->app)))
        caller->app[0] = '\0';
    return 0;
}

static bool app_granted(const char *app, const char *permission)
{
    char path[PATH_LEN], text[PERMISSIONS_BUF];
    snprintf(path, sizeof(path), "%s/%s.permissions", sys.permissions_dir, app);
    FILE *file = fopen(path, "r");
    if (!file)
        return false;
    size_t n = fread(text, 1, sizeof(text) - 1, file);
    fclose(file);
    text[n] = '\0';
    size_t len = strlen(permission);
    for (const char *p = text; (p = strstr(p, permission)); p += len)
        if ((p == text || isspace((unsigned char)p[-1])) && (p[len] == '\0' || isspace((unsigned char)p[len])))
            return true;
    return false;
}

static int require_shell(sd_bus_message *m, sd_bus_error *error)
{
    caller_t caller;
    if (caller_of(m, &caller) < 0 || !caller.shell)
        return sd_bus_error_set(error, SD_BUS_ERROR_ACCESS_DENIED, "Only the Slate shell may do this");
    return 0;
}

static int require_app(sd_bus_message *m, const char *permission, caller_t *caller, sd_bus_error *error)
{
    if (caller_of(m, caller) < 0 || !caller->app[0] || !app_granted(caller->app, permission))
        return sd_bus_error_setf(error, SD_BUS_ERROR_ACCESS_DENIED, "This needs the %s permission", permission);
    return 0;
}

/* ---- key store -------------------------------------------------------------------------- */

static int keystore_caller(sd_bus_message *m, char *ns, size_t len, sd_bus_error *error)
{
    caller_t caller;
    if (caller_of(m, &caller) < 0 || !(caller.shell || (caller.app[0] && app_granted(caller.app, "secrets"))) ||
        !keystore_namespace(caller.user, ns, len))
        return sd_bus_error_set(error, SD_BUS_ERROR_ACCESS_DENIED, "This needs the secrets permission");
    return 0;
}

static int read_key(sd_bus_message *m, const char **key, sd_bus_error *error)
{
    int r = sd_bus_message_read(m, "s", key);
    if (r < 0)
        return r;
    if (!keystore_valid_key(*key))
        return sd_bus_error_set(error, SD_BUS_ERROR_INVALID_ARGS, "Keys are 1-128 letters, digits, '.', '_' or '-'");
    return 0;
}

static int keystore_get(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
    (void)userdata;
    char ns[KEYSTORE_NAMESPACE_MAX];
    const char *key;
    int r = keystore_caller(m, ns, sizeof(ns), error);
    if (r < 0 || (r = read_key(m, &key, error)) < 0)
        return r;
    uint8_t *value = NULL;
    size_t len = 0;
    r = vault_get(ns, key, &value, &len);
    if (r == -ENOENT)
        return sd_bus_error_set(error, ERROR_NOT_FOUND, "No such key");
    if (r < 0)
        return sd_bus_error_set(error, SD_BUS_ERROR_IO_ERROR, "The key couldn't be read");
    sd_bus_message *reply = NULL;
    r = sd_bus_message_new_method_return(m, &reply);
    if (r >= 0)
        r = sd_bus_message_append_array(reply, 'y', value, len);
    if (r >= 0)
        r = sd_bus_send(NULL, reply, NULL);
    sodium_memzero(value, len);
    free(value);
    sd_bus_message_unref(reply);
    return r;
}

static int keystore_set(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
    (void)userdata;
    char ns[KEYSTORE_NAMESPACE_MAX];
    const char *key;
    const void *value;
    size_t len;
    int r = keystore_caller(m, ns, sizeof(ns), error);
    if (r < 0 || (r = read_key(m, &key, error)) < 0 || (r = sd_bus_message_read_array(m, 'y', &value, &len)) < 0)
        return r;
    if (len > KEYSTORE_VALUE_MAX)
        return sd_bus_error_set(error, SD_BUS_ERROR_INVALID_ARGS, "Values are at most 64 KiB");
    r = vault_set(ns, key, value, len);
    if (r == -ENOSPC)
        return sd_bus_error_set(error, ERROR_FULL, "This app already stores the most keys allowed");
    if (r < 0)
        return sd_bus_error_set(error, SD_BUS_ERROR_IO_ERROR, "The key couldn't be saved");
    return sd_bus_reply_method_return(m, "");
}

static int keystore_delete(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
    (void)userdata;
    char ns[KEYSTORE_NAMESPACE_MAX];
    const char *key;
    int r = keystore_caller(m, ns, sizeof(ns), error);
    if (r < 0 || (r = read_key(m, &key, error)) < 0)
        return r;
    r = vault_delete(ns, key);
    if (r == -ENOENT)
        return sd_bus_error_set(error, ERROR_NOT_FOUND, "No such key");
    if (r < 0)
        return sd_bus_error_set(error, SD_BUS_ERROR_IO_ERROR, "The key couldn't be deleted");
    return sd_bus_reply_method_return(m, "");
}

static void append_key(const char *key, void *user)
{
    sd_bus_message_append((sd_bus_message *)user, "s", key);
}

static int keystore_list(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
    (void)userdata;
    char ns[KEYSTORE_NAMESPACE_MAX];
    int r = keystore_caller(m, ns, sizeof(ns), error);
    if (r < 0)
        return r;
    sd_bus_message *reply = NULL;
    r = sd_bus_message_new_method_return(m, &reply);
    if (r >= 0)
        r = sd_bus_message_open_container(reply, 'a', "s");
    if (r >= 0) {
        vault_list(ns, append_key, reply);
        r = sd_bus_message_close_container(reply);
    }
    if (r >= 0)
        r = sd_bus_send(NULL, reply, NULL);
    sd_bus_message_unref(reply);
    return r;
}

static const sd_bus_vtable keystore_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD_WITH_ARGS("Get", SD_BUS_ARGS("s", key), SD_BUS_RESULT("ay", value), keystore_get,
                            SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD_WITH_ARGS("Set", SD_BUS_ARGS("s", key, "ay", value), SD_BUS_NO_RESULT, keystore_set,
                            SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD_WITH_ARGS("Delete", SD_BUS_ARGS("s", key), SD_BUS_NO_RESULT, keystore_delete,
                            SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD_WITH_ARGS("List", SD_BUS_NO_ARGS, SD_BUS_RESULT("as", keys), keystore_list,
                            SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END,
};

/* ---- voice assistant -------------------------------------------------------------------- */

static void active_path(char *out, size_t len)
{
    snprintf(out, len, "%s/assistant", sys.state_dir);
}

static void load_active(void)
{
    char path[PATH_LEN];
    active_path(path, sizeof(path));
    FILE *file = fopen(path, "r");
    char id[ACCESS_ID_MAX] = "";
    if (file) {
        if (!fgets(id, sizeof(id), file))
            id[0] = '\0';
        fclose(file);
    }
    id[strcspn(id, "\n")] = '\0';
    char user[ACCESS_ID_MAX + 8], check[ACCESS_ID_MAX];
    snprintf(user, sizeof(user), ACCESS_APP_PREFIX "%s", id);
    snprintf(sys.active, sizeof(sys.active), "%s", id[0] && access_app_from_user(user, check, sizeof(check)) ? id : "");
}

static int set_active(const char *id)
{
    if (strcmp(sys.active, id) == 0)
        return 0;
    snprintf(sys.active, sizeof(sys.active), "%s", id);
    char path[PATH_LEN], tmp[PATH_LEN + 8];
    active_path(path, sizeof(path));
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *file = fopen(tmp, "w");
    if (file) {
        fprintf(file, "%s\n", id);
        if (fclose(file) == 0)
            rename(tmp, path);
    }
    return sd_bus_emit_properties_changed(sys.bus, ASSISTANT_PATH, ASSISTANT_IFACE, "Active", NULL);
}

static status_t *status_for(const char *id)
{
    status_t *free_slot = NULL;
    for (size_t i = 0; i < STATUS_MAX; i++) {
        if (strcmp(sys.statuses[i].id, id) == 0)
            return &sys.statuses[i];
        if (!sys.statuses[i].id[0] && !free_slot)
            free_slot = &sys.statuses[i];
    }
    if (free_slot) {
        memset(free_slot, 0, sizeof(*free_slot));
        snprintf(free_slot->id, sizeof(free_slot->id), "%s", id);
        snprintf(free_slot->activity, sizeof(free_slot->activity), ASSISTANT_ACTIVITY_IDLE);
    }
    return free_slot;
}

static int assistant_set_active(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
    (void)userdata;
    const char *id;
    int r = require_shell(m, error);
    if (r < 0 || (r = sd_bus_message_read(m, "s", &id)) < 0)
        return r;
    char user[ACCESS_ID_MAX + 8], check[ACCESS_ID_MAX];
    snprintf(user, sizeof(user), ACCESS_APP_PREFIX "%s", id);
    if (id[0] && (!access_app_from_user(user, check, sizeof(check)) || !app_granted(id, "assistant")))
        return sd_bus_error_set(error, SD_BUS_ERROR_INVALID_ARGS, "Not an installed assistant");
    if ((r = set_active(id)) < 0)
        return r;
    return sd_bus_reply_method_return(m, "");
}

static int assistant_request_active(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
    (void)userdata;
    caller_t caller;
    const char *reason;
    int r = require_app(m, "assistant", &caller, error);
    if (r < 0 || (r = sd_bus_message_read(m, "s", &reason)) < 0)
        return r;
    if (!assistant_valid_text(reason, ASSISTANT_REASON_MAX))
        return sd_bus_error_set(error, SD_BUS_ERROR_INVALID_ARGS, "The reason is one short line");
    /* Marvin and Ava are trusted, so a request takes effect at once; the shell tells the user. */
    bool changed = strcmp(sys.active, caller.app) != 0;
    if ((r = set_active(caller.app)) < 0)
        return r;
    if (changed)
        sd_bus_emit_signal(sys.bus, ASSISTANT_PATH, ASSISTANT_IFACE, "ActiveRequested", "ss", caller.app, reason);
    return sd_bus_reply_method_return(m, "");
}

static int assistant_report_status(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
    (void)userdata;
    caller_t caller;
    const char *state_name, *detail;
    assistant_state_t state;
    int r = require_app(m, "assistant", &caller, error);
    if (r < 0 || (r = sd_bus_message_read(m, "ss", &state_name, &detail)) < 0)
        return r;
    if (!assistant_parse_state(state_name, &state) || !assistant_valid_text(detail, ASSISTANT_DETAIL_MAX))
        return sd_bus_error_set(error, SD_BUS_ERROR_INVALID_ARGS, "Unknown state or detail too long");
    status_t *status = status_for(caller.app);
    if (!status)
        return sd_bus_error_set(error, SD_BUS_ERROR_LIMITS_EXCEEDED, "Too many assistants");
    status->state = state;
    snprintf(status->detail, sizeof(status->detail), "%s", detail);
    sd_bus_emit_signal(sys.bus, ASSISTANT_PATH, ASSISTANT_IFACE, "StatusChanged", "sss", caller.app, state_name,
                       detail);
    return sd_bus_reply_method_return(m, "");
}

static int assistant_report_activity(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
    (void)userdata;
    caller_t caller;
    const char *activity;
    int r = require_app(m, "assistant", &caller, error);
    if (r < 0 || (r = sd_bus_message_read(m, "s", &activity)) < 0)
        return r;
    if (!assistant_valid_activity(activity))
        return sd_bus_error_set(error, SD_BUS_ERROR_INVALID_ARGS, "Activity is capturing or idle");
    status_t *status = status_for(caller.app);
    if (!status)
        return sd_bus_error_set(error, SD_BUS_ERROR_LIMITS_EXCEEDED, "Too many assistants");
    snprintf(status->activity, sizeof(status->activity), "%s", activity);
    sd_bus_emit_signal(sys.bus, ASSISTANT_PATH, ASSISTANT_IFACE, "ActivityChanged", "ss", caller.app, activity);
    return sd_bus_reply_method_return(m, "");
}

static int assistant_get_statuses(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
    (void)userdata;
    int r = require_shell(m, error);
    if (r < 0)
        return r;
    sd_bus_message *reply = NULL;
    r = sd_bus_message_new_method_return(m, &reply);
    if (r >= 0)
        r = sd_bus_message_open_container(reply, 'a', "(ssss)");
    for (size_t i = 0; r >= 0 && i < STATUS_MAX; i++) {
        const status_t *s = &sys.statuses[i];
        if (s->id[0])
            r = sd_bus_message_append(reply, "(ssss)", s->id, assistant_state_name(s->state), s->detail, s->activity);
    }
    if (r >= 0)
        r = sd_bus_message_close_container(reply);
    if (r >= 0)
        r = sd_bus_send(NULL, reply, NULL);
    sd_bus_message_unref(reply);
    return r;
}

static int request_flow(sd_bus_message *m, const char *signal, sd_bus_error *error)
{
    const char *id, *token;
    int r = require_shell(m, error);
    if (r < 0 || (r = sd_bus_message_read(m, "ss", &id, &token)) < 0)
        return r;
    if (!app_granted(id, "assistant"))
        return sd_bus_error_set(error, SD_BUS_ERROR_INVALID_ARGS, "Not an installed assistant");
    r = sd_bus_emit_signal(sys.bus, ASSISTANT_PATH, ASSISTANT_IFACE, signal, "ss", id, token);
    return r < 0 ? r : sd_bus_reply_method_return(m, "");
}

static int assistant_request_setup(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
    (void)userdata;
    return request_flow(m, "SetupRequested", error);
}

static int assistant_request_logout(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
    (void)userdata;
    return request_flow(m, "LogoutRequested", error);
}

static int assistant_get_active(sd_bus *bus, const char *path, const char *interface, const char *property,
                                sd_bus_message *reply, void *userdata, sd_bus_error *error)
{
    (void)bus, (void)path, (void)interface, (void)property, (void)userdata, (void)error;
    return sd_bus_message_append(reply, "s", sys.active);
}

static const sd_bus_vtable assistant_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Active", "s", assistant_get_active, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_METHOD_WITH_ARGS("SetActive", SD_BUS_ARGS("s", id), SD_BUS_NO_RESULT, assistant_set_active,
                            SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD_WITH_ARGS("RequestActive", SD_BUS_ARGS("s", reason), SD_BUS_NO_RESULT, assistant_request_active,
                            SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD_WITH_ARGS("ReportStatus", SD_BUS_ARGS("s", state, "s", detail), SD_BUS_NO_RESULT,
                            assistant_report_status, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD_WITH_ARGS("ReportActivity", SD_BUS_ARGS("s", activity), SD_BUS_NO_RESULT,
                            assistant_report_activity, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD_WITH_ARGS("GetStatuses", SD_BUS_NO_ARGS, SD_BUS_RESULT("a(ssss)", statuses),
                            assistant_get_statuses, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD_WITH_ARGS("RequestSetup", SD_BUS_ARGS("s", id, "s", token), SD_BUS_NO_RESULT,
                            assistant_request_setup, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD_WITH_ARGS("RequestLogout", SD_BUS_ARGS("s", id, "s", token), SD_BUS_NO_RESULT,
                            assistant_request_logout, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL_WITH_ARGS("ActiveRequested", SD_BUS_ARGS("s", id, "s", reason), 0),
    SD_BUS_SIGNAL_WITH_ARGS("StatusChanged", SD_BUS_ARGS("s", id, "s", state, "s", detail), 0),
    SD_BUS_SIGNAL_WITH_ARGS("ActivityChanged", SD_BUS_ARGS("s", id, "s", activity), 0),
    SD_BUS_SIGNAL_WITH_ARGS("SetupRequested", SD_BUS_ARGS("s", id, "s", token), 0),
    SD_BUS_SIGNAL_WITH_ARGS("LogoutRequested", SD_BUS_ARGS("s", id, "s", token), 0),
    SD_BUS_VTABLE_END,
};

/* ---- main ------------------------------------------------------------------------------- */

int main(void)
{
    sys.state_dir = getenv("SLATE_SYSTEM_STATE_DIR");
    if (!sys.state_dir)
        sys.state_dir = DEFAULT_STATE_DIR;
    sys.permissions_dir = getenv("SLATE_SYSTEM_PERMISSIONS_DIR");
    if (!sys.permissions_dir)
        sys.permissions_dir = DEFAULT_PERMISSIONS_DIR;

    char keystore_dir[PATH_LEN];
    snprintf(keystore_dir, sizeof(keystore_dir), "%s/keystore", sys.state_dir);
    if (!vault_open(keystore_dir)) {
        fprintf(stderr, "slate-system: couldn't open the key store in %s\n", keystore_dir);
        return 1;
    }
    load_active();

    int r = sd_bus_open_system(&sys.bus);
    if (r >= 0)
        r = sd_bus_add_object_vtable(sys.bus, NULL, KEYSTORE_PATH, KEYSTORE_IFACE, keystore_vtable, NULL);
    if (r >= 0)
        r = sd_bus_add_object_vtable(sys.bus, NULL, ASSISTANT_PATH, ASSISTANT_IFACE, assistant_vtable, NULL);
    if (r >= 0)
        r = sd_bus_request_name(sys.bus, BUS_NAME, 0);
    if (r < 0) {
        fprintf(stderr, "slate-system: couldn't serve %s: %s\n", BUS_NAME, strerror(-r));
        return 1;
    }
    for (;;) {
        r = sd_bus_process(sys.bus, NULL);
        if (r < 0) {
            fprintf(stderr, "slate-system: bus error: %s\n", strerror(-r));
            return 1;
        }
        if (r > 0)
            continue;
        if (sd_bus_wait(sys.bus, UINT64_MAX) < 0)
            return 1;
    }
}
