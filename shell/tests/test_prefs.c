#include <string.h>

#include "check.h"
#include "prefs.h"

static void test_update(void)
{
    char out[128];
    CHECK(prefs_update("", "screen_off_seconds", "300", out, sizeof(out)));
    CHECK(strcmp(out, "screen_off_seconds=300\n") == 0);

    CHECK(prefs_update("a=1\nscreen_off_seconds=60\nb=2\n", "screen_off_seconds", "0", out, sizeof(out)));
    CHECK(strcmp(out, "a=1\nb=2\nscreen_off_seconds=0\n") == 0);

    CHECK(prefs_update("a=1", "b", "2", out, sizeof(out)));
    CHECK(strcmp(out, "a=1\nb=2\n") == 0);

    CHECK(prefs_update("ab=1\n", "a", "2", out, sizeof(out)));
    CHECK(strcmp(out, "ab=1\na=2\n") == 0);

    CHECK(!prefs_update("a=1\n", "key", "value", out, 8));
}

int main(void)
{
    test_update();
    return failures ? 1 : 0;
}
