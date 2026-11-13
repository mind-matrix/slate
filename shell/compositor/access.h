#ifndef SLATE_COMPOSITOR_ACCESS_H
#define SLATE_COMPOSITOR_ACCESS_H

#include <stdbool.h>
#include <stddef.h>

#define ACCESS_APP_PREFIX "app-"
#define ACCESS_PERMISSIONS_DIR "/run/slate/apps"
#define ACCESS_ID_MAX 32
#define ACCESS_PERMISSIONS_MAX 256

/* Turns the user an app runs as ("app-ava") into its app id ("ava"). */
bool access_app_from_user(const char *user, char *id, size_t len);

/* Whether a client with these granted permissions may see a Wayland global. Apps see only what
 * their window needs: xdg_wm_base needs gui, the layer shell needs overlay, and the core
 * globals need one of the two. Anything not listed here is never offered to an app. */
bool access_global_allowed(const char *interface, const char *permissions);

#endif
