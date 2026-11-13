#include <string.h>

#include "access.h"
#include "check.h"

static void test_app_from_user(void)
{
    char id[ACCESS_ID_MAX];
    CHECK(access_app_from_user("app-ava", id, sizeof(id)) && strcmp(id, "ava") == 0);
    CHECK(access_app_from_user("app-hello-2", id, sizeof(id)) && strcmp(id, "hello-2") == 0);
    CHECK(!access_app_from_user("slate", id, sizeof(id)));
    CHECK(!access_app_from_user("app-", id, sizeof(id)));
    CHECK(!access_app_from_user("app-Ava", id, sizeof(id)));
    CHECK(!access_app_from_user("app-a/b", id, sizeof(id)));
    CHECK(!access_app_from_user("app--x", id, sizeof(id)));
    CHECK(!access_app_from_user(NULL, id, sizeof(id)));
    CHECK(!access_app_from_user("app-abcdefghijklmnopqrstuvwxyz0123456", id, sizeof(id)));
}

static void test_globals(void)
{
    CHECK(access_global_allowed("xdg_wm_base", "network gui"));
    CHECK(!access_global_allowed("xdg_wm_base", "overlay"));
    CHECK(access_global_allowed("zwlr_layer_shell_v1", "overlay audio"));
    CHECK(!access_global_allowed("zwlr_layer_shell_v1", "gui"));
    CHECK(access_global_allowed("wl_compositor", "gui"));
    CHECK(access_global_allowed("wl_seat", "overlay"));
    CHECK(!access_global_allowed("wl_compositor", "network audio"));
    CHECK(!access_global_allowed("wl_compositor", ""));
    CHECK(!access_global_allowed("zwlr_screencopy_manager_v1", "gui overlay"));
    CHECK(!access_global_allowed("xdg_wm_base", "guide"));
    CHECK(access_global_allowed("xdg_wm_base", "gui\n"));
}

int main(void)
{
    test_app_from_user();
    test_globals();
    return failures ? 1 : 0;
}
