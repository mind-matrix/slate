#include <fcntl.h>
#include <pwd.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "bus.h"
#include "voice.h"

#define SERVICE "org.slate.System"
#define PATH "/org/slate/Assistant"
#define IFACE "org.slate.Assistant1"
#define USERS_MAX 512

extern char **environ;

static struct {
    voice_changed_cb_t changed;
    voice_requested_cb_t requested;
    void *user;
    sd_bus_slot *signals;
    sd_bus_slot *properties;
    FILE *monitor;
} watch;

static void read_statuses(voice_t *out, size_t count)
{
    sd_bus *bus = bus_system();
    sd_bus_message *reply = NULL;
    if (!bus || sd_bus_call_method(bus, SERVICE, PATH, IFACE, "GetStatuses", NULL, &reply, "") < 0)
        return;
    const char *id, *state, *detail, *activity;
    if (sd_bus_message_enter_container(reply, 'a', "(ssss)") >= 0) {
        while (sd_bus_message_read(reply, "(ssss)", &id, &state, &detail, &activity) > 0)
            for (size_t i = 0; i < count; i++)
                if (strcmp(out[i].app.id, id) == 0 && assistant_parse_state(state, &out[i].state)) {
                    out[i].reported = true;
                    snprintf(out[i].detail, sizeof(out[i].detail), "%s", detail);
                    out[i].capturing = strcmp(activity, ASSISTANT_ACTIVITY_CAPTURING) == 0;
                }
        sd_bus_message_exit_container(reply);
    }
    sd_bus_message_unref(reply);
}

size_t voice_list(voice_t *out, size_t max)
{
    app_t apps[APPS_MAX];
    size_t count = apps_list(apps, APPS_MAX), n = 0;
    for (size_t i = 0; i < count && n < max; i++) {
        if (!apps_has(apps[i].granted, "assistant"))
            continue;
        memset(&out[n], 0, sizeof(out[n]));
        out[n].app = apps[i];
        out[n].running = apps_running(apps[i].id);
        n++;
    }
    read_statuses(out, n);
    return n;
}

bool voice_get_active(char *out, size_t len)
{
    return bus_get_text(SERVICE, PATH, IFACE, "Active", 's', out, len);
}

static bool call(const char *method, const char *types, const char *a, const char *b)
{
    sd_bus *bus = bus_system();
    sd_bus_error error = SD_BUS_ERROR_NULL;
    int r = bus ? (b ? sd_bus_call_method(bus, SERVICE, PATH, IFACE, method, &error, NULL, types, a, b)
                     : sd_bus_call_method(bus, SERVICE, PATH, IFACE, method, &error, NULL, types, a))
                : -1;
    if (r < 0)
        fprintf(stderr, "slate: %s failed: %s\n", method, error.message ? error.message : "no bus");
    sd_bus_error_free(&error);
    return r >= 0;
}

bool voice_set_active(const char *id)
{
    return call("SetActive", "s", id, NULL);
}

bool voice_request(const char *id, voice_action_t action, const char *token)
{
    return call(action == VOICE_ACTION_LOGOUT ? "RequestLogout" : "RequestSetup", "ss", id, token);
}

static int on_signal(sd_bus_message *m, void *userdata, sd_bus_error *error)
{
    (void)userdata;
    (void)error;
    const char *id, *reason;
    if (sd_bus_message_is_signal(m, IFACE, "ActiveRequested") && sd_bus_message_read(m, "ss", &id, &reason) >= 0 &&
        watch.requested)
        watch.requested(id, reason, watch.user);
    if (watch.changed)
        watch.changed(watch.user);
    return 0;
}

void voice_watch(voice_changed_cb_t changed, voice_requested_cb_t requested, void *user)
{
    watch.changed = changed;
    watch.requested = requested;
    watch.user = user;
    sd_bus *bus = bus_system();
    if (!bus || watch.signals)
        return;
    sd_bus_match_signal(bus, &watch.signals, SERVICE, PATH, IFACE, NULL, on_signal, NULL);
    sd_bus_match_signal(bus, &watch.properties, SERVICE, PATH, "org.freedesktop.DBus.Properties",
                        "PropertiesChanged", on_signal, NULL);
}

static void set_metadata(const char *key, const char *value)
{
    char *argv[] = { "pw-metadata", "-n", "default", "0", (char *)key, (char *)value, NULL };
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    pid_t pid;
    if (posix_spawnp(&pid, "pw-metadata", &actions, NULL, argv, environ) == 0)
        waitpid(pid, NULL, 0);
    posix_spawn_file_actions_destroy(&actions);
}

void voice_sync_audio(void)
{
    voice_t voices[VOICE_MAX];
    size_t count = voice_list(voices, VOICE_MAX), used = 0;
    char users[USERS_MAX] = "", active[APPS_ID_MAX] = "";
    for (size_t i = 0; i < count && used < sizeof(users); i++) {
        char name[APPS_ID_MAX + 8];
        snprintf(name, sizeof(name), "app-%.*s", APPS_ID_MAX - 1, voices[i].app.id);
        struct passwd *pw = getpwnam(name);
        if (pw)
            used += (size_t)snprintf(users + used, sizeof(users) - used, "%s%u:%s", used ? " " : "",
                                     (unsigned)pw->pw_uid, voices[i].app.id);
    }
    voice_get_active(active, sizeof(active));
    set_metadata("slate.assistant.users", users);
    set_metadata("slate.assistant.active", active);
}

int voice_mic_monitor(void)
{
    if (watch.monitor)
        pclose(watch.monitor);
    watch.monitor = popen("exec pw-metadata -m -n default 2>/dev/null", "r");
    if (!watch.monitor)
        return -1;
    int fd = fileno(watch.monitor);
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    return fd;
}
