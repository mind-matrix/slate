#include "platform/windows.h"

#include <stdlib.h>
#include <string.h>

/* The simulator and snapshots have no compositor. SLATE_FAKE_FRONT names an app to treat as
 * being in front, so the status bar's app controls can be previewed. */

static sui_windows_cb_t change_cb;
static void *change_user;

bool sui_windows_show(const char *app)
{
    (void)app;
    return false;
}

void sui_windows_expect(const char *app)
{
    (void)app;
}

const char *sui_windows_front(void)
{
    const char *front = getenv("SLATE_FAKE_FRONT");
    return front && front[0] ? front : NULL;
}

void sui_windows_back(void)
{
}

void sui_windows_home(void)
{
    unsetenv("SLATE_FAKE_FRONT");
    if (change_cb)
        change_cb(change_user);
}

bool sui_windows_token(const char *app, char *out, size_t len)
{
    (void)app;
    if (len)
        out[0] = '\0';
    return false;
}

void sui_windows_on_change(sui_windows_cb_t cb, void *user)
{
    change_cb = cb;
    change_user = user;
}

lv_obj_t *sui_layer_system(void)
{
    return lv_layer_top();
}
