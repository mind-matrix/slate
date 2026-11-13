#ifndef SUI_PLATFORM_WINDOWS_H
#define SUI_PLATFORM_WINDOWS_H

#include <stdbool.h>
#include <stddef.h>
#include "lvgl.h"

/* App windows. On the device the shell is the Wayland compositor and apps with the gui
 * permission draw their own full-screen windows below the status bar. An app's window only
 * comes to the front through Slate: a launcher tap, Open in app details, or an activation
 * token Slate hands to the app. In the simulator there are no app windows. */

typedef void (*sui_windows_cb_t)(void *user);

/* Brings the app's window to the front. False when the app has no window yet. */
bool sui_windows_show(const char *app);
/* The app's next window comes to the front if it appears in the next few seconds. Used when
 * Slate starts the app because it was opened. */
void sui_windows_expect(const char *app);
/* The app whose window is in front, or NULL when the shell's own screens are showing. */
const char *sui_windows_front(void);
/* System Back: asks the app in front to close its window. Apps handle it to go back a step
 * and close the window when there is nothing to go back to. */
void sui_windows_back(void);
/* System Home: hides the app's window and shows the desktop. The app keeps running. */
void sui_windows_home(void);
/* A one-time activation token that lets the app bring its own window to the front. */
bool sui_windows_token(const char *app, char *out, size_t len);
void sui_windows_on_change(sui_windows_cb_t cb, void *user);

/* Parent for system UI that sits above app windows: status bar, drawer, dialogs and alerts. */
lv_obj_t *sui_layer_system(void);

#endif
