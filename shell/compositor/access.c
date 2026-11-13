#include "access.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char *const core_globals[] = {
    "wl_compositor",
    "wl_subcompositor",
    "wl_shm",
    "wl_seat",
    "wl_output",
    "wl_drm",
    "zwp_linux_dmabuf_v1",
    "xdg_activation_v1",
    "wp_viewporter",
    "wp_presentation",
};

static bool has_word(const char *list, const char *word)
{
    size_t n = strlen(word);
    for (const char *p = list; p && (p = strstr(p, word)); p += n)
        if ((p == list || isspace((unsigned char)p[-1])) && (p[n] == '\0' || isspace((unsigned char)p[n])))
            return true;
    return false;
}

bool access_app_from_user(const char *user, char *id, size_t len)
{
    size_t prefix = strlen(ACCESS_APP_PREFIX);
    if (!user || strncmp(user, ACCESS_APP_PREFIX, prefix) != 0)
        return false;
    const char *name = user + prefix;
    size_t n = strlen(name);
    if (n == 0 || n >= ACCESS_ID_MAX || n >= len || !(islower((unsigned char)name[0]) || isdigit((unsigned char)name[0])))
        return false;
    for (size_t i = 0; i < n; i++)
        if (!(islower((unsigned char)name[i]) || isdigit((unsigned char)name[i]) || name[i] == '-'))
            return false;
    snprintf(id, len, "%s", name);
    return true;
}

bool access_global_allowed(const char *interface, const char *permissions)
{
    bool gui = has_word(permissions, "gui");
    bool overlay = has_word(permissions, "overlay");
    if (strcmp(interface, "xdg_wm_base") == 0)
        return gui;
    if (strcmp(interface, "zwlr_layer_shell_v1") == 0)
        return overlay;
    for (size_t i = 0; i < sizeof(core_globals) / sizeof(core_globals[0]); i++)
        if (strcmp(interface, core_globals[i]) == 0)
            return gui || overlay;
    return false;
}
