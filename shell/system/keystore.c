#include "keystore.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "access.h"

bool keystore_valid_key(const char *key)
{
    if (!key || !key[0] || key[0] == '.')
        return false;
    size_t n = 0;
    for (; key[n]; n++) {
        char c = key[n];
        if (!(isalnum((unsigned char)c) || c == '.' || c == '_' || c == '-'))
            return false;
    }
    return n <= KEYSTORE_KEY_MAX;
}

bool keystore_namespace(const char *user, char *out, size_t len)
{
    if (!user)
        return false;
    if (strcmp(user, KEYSTORE_SHELL_USER) == 0) {
        snprintf(out, len, "%s", KEYSTORE_SHELL_NAMESPACE);
        return true;
    }
    char id[ACCESS_ID_MAX];
    if (!access_app_from_user(user, id, sizeof(id)) || strlen(ACCESS_APP_PREFIX) + strlen(id) >= len)
        return false;
    /* Apps keep the "app-" prefix so an app called "shell" can't share the shell's namespace. */
    snprintf(out, len, ACCESS_APP_PREFIX "%s", id);
    return true;
}
