#include <string.h>

#include "apps.h"
#include "check.h"

static void test_ids(void)
{
    CHECK(apps_valid_id("ava"));
    CHECK(apps_valid_id("hello-2"));
    CHECK(!apps_valid_id("Ava"));
    CHECK(!apps_valid_id("-ava"));
    CHECK(!apps_valid_id("../ava"));
    CHECK(!apps_valid_id(""));
    CHECK(!apps_valid_id("a-very-long-app-id-that-goes-past-the-limit"));
}

static void test_parse(void)
{
    app_t app;
    apps_parse_app("name=Ava Assistant\npermissions=audio network gui\nregistration=required\n", "ava", &app);
    CHECK(strcmp(app.id, "ava") == 0);
    CHECK(strcmp(app.name, "Ava Assistant") == 0);
    CHECK(strcmp(app.requested, "audio network gui") == 0);
    CHECK(app.registration);

    apps_parse_app("permissions=network\n", "hello", &app);
    CHECK(strcmp(app.name, "hello") == 0);
    CHECK(!app.registration);
    apps_parse_app("name=\nregistration=none\n", "marvin", &app);
    CHECK(strcmp(app.name, "marvin") == 0 && !app.registration);
}

static void test_lists(void)
{
    CHECK(apps_has("audio network gui", "network"));
    CHECK(apps_has("audio network gui", "audio"));
    CHECK(apps_has("audio network gui", "gui"));
    CHECK(!apps_has("audio network gui", "net"));
    CHECK(!apps_has("audio networking", "network"));
    CHECK(!apps_has("", "gui"));
    CHECK(apps_count("audio network gui") == 3);
    CHECK(apps_count("  audio   gui\n") == 2);
    CHECK(apps_count("") == 0);
}

static void test_sort(void)
{
    app_t apps[3] = { { .name = "marvin" }, { .name = "Ava" }, { .name = "Hello" } };
    apps_sort(apps, 3);
    CHECK(strcmp(apps[0].name, "Ava") == 0 && strcmp(apps[2].name, "marvin") == 0);
}

int main(void)
{
    test_ids();
    test_parse();
    test_lists();
    test_sort();
    return failures ? 1 : 0;
}
