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

static void test_app_line(void)
{
    update_app_line_t line;
    update_status_t status = { .state = UPDATE_AVAILABLE, .version = "0.9.4", .size = 6291456 };
    update_app_line(&status, "0.9.3", false, &line);
    CHECK(strcmp(line.text, "Version 0.9.4 available · 6 MB") == 0);
    CHECK(line.tone == UPDATE_TONE_ACCENT && line.action && strcmp(line.action, "Update") == 0);

    update_app_line(&status, "0.9.3-dev", true, &line);
    CHECK(strcmp(line.text, "Version 0.9.3-dev · dev build, not updated") == 0);
    CHECK(line.tone == UPDATE_TONE_MUTED && !line.action);

    status = (update_status_t){ .state = UPDATE_DOWNLOADING, .version = "0.9.4", .progress = 40 };
    update_app_line(&status, "0.9.3", false, &line);
    CHECK(strcmp(line.text, "Downloading 0.9.4 · 40%") == 0 && !line.action);

    status = (update_status_t){ .state = UPDATE_ERROR };
    update_app_line(&status, "0.9.3", false, &line);
    CHECK(line.tone == UPDATE_TONE_DANGER && strcmp(line.action, "Try again") == 0);

    status = (update_status_t){ .state = UPDATE_CURRENT };
    update_app_line(&status, "", false, &line);
    CHECK(strcmp(line.text, "Version unknown · up to date") == 0);

    status = (update_status_t){ .state = UPDATE_UNKNOWN };
    update_app_line(&status, "1.2.0", false, &line);
    CHECK(strcmp(line.text, "Version 1.2.0") == 0 && !line.action);
}

int main(void)
{
    test_parse_status();
    test_parse_notes();
    test_check_due();
    test_format_size();
    test_app_line();
    return failures ? 1 : 0;
}
