#include <string.h>

#include "check.h"
#include "update.h"

static void test_parse_status(void)
{
    update_status_t status;
    update_parse_status("state=downloading\nversion=2026.11.20\nsize=18874368\nprogress=42\nchecked=1762500000\n",
                        &status);
    CHECK(status.state == UPDATE_DOWNLOADING);
    CHECK(strcmp(status.version, "2026.11.20") == 0);
    CHECK(status.size == 18874368);
    CHECK(status.progress == 42);
    CHECK(status.checked == 1762500000);

    update_parse_status("state=error\nreason=unverified\nchecked=0\n", &status);
    CHECK(status.state == UPDATE_ERROR);
    CHECK(strcmp(status.reason, "unverified") == 0);
    CHECK(status.version[0] == '\0');

    update_parse_status("state=downloading\nprogress=250\n", &status);
    CHECK(status.progress == 100);

    update_parse_status("state=unexpected\n", &status);
    CHECK(status.state == UPDATE_UNKNOWN);
    update_parse_status("", &status);
    CHECK(status.state == UPDATE_UNKNOWN && status.checked == 0);
}

static void test_parse_notes(void)
{
    update_note_t notes[3];
    CHECK(update_parse_notes("First\n\nSecond\nThird\nFourth\n", notes, 3) == 3);
    CHECK(strcmp(notes[0], "First") == 0);
    CHECK(strcmp(notes[1], "Second") == 0);
    CHECK(strcmp(notes[2], "Third") == 0);
    CHECK(update_parse_notes("", notes, 3) == 0);
}

static void test_check_due(void)
{
    update_status_t status = { .state = UPDATE_CURRENT, .checked = 1000 };
    CHECK(!update_check_due(&status, 1000 + UPDATE_DAY_SECONDS - 1));
    CHECK(update_check_due(&status, 1000 + UPDATE_DAY_SECONDS));
    status.state = UPDATE_UNKNOWN;
    status.checked = 0;
    CHECK(update_check_due(&status, 1762500000));
    status.state = UPDATE_RESTART;
    CHECK(!update_check_due(&status, 1762500000));
    status.state = UPDATE_DISABLED;
    CHECK(!update_check_due(&status, 1762500000));
    status.state = UPDATE_DOWNLOADING;
    CHECK(!update_check_due(&status, 1762500000));
}

static void test_format_size(void)
{
    char out[16];
    update_format_size(18874368, out, sizeof(out));
    CHECK(strcmp(out, "18 MB") == 0);
    update_format_size(4096, out, sizeof(out));
    CHECK(strcmp(out, "4 KB") == 0);
}

int main(void)
{
    test_parse_status();
    test_parse_notes();
    test_check_due();
    test_format_size();
    return failures ? 1 : 0;
}
