#include <string.h>

#include "assistant.h"
#include "check.h"

static void test_states(void)
{
    assistant_state_t state;
    CHECK(assistant_parse_state("registered", &state) && state == ASSISTANT_REGISTERED);
    CHECK(assistant_parse_state("unregistering", &state) && state == ASSISTANT_UNREGISTERING);
    CHECK(assistant_parse_state("error", &state) && state == ASSISTANT_ERROR);
    CHECK(!assistant_parse_state("Registered", &state));
    CHECK(!assistant_parse_state("", &state));
    CHECK(!assistant_parse_state(NULL, &state));
    for (int i = 0; i < ASSISTANT_STATE_COUNT; i++)
        CHECK(assistant_parse_state(assistant_state_name((assistant_state_t)i), &state) && state == (assistant_state_t)i);
}

static void test_activity_and_text(void)
{
    CHECK(assistant_valid_activity("capturing"));
    CHECK(assistant_valid_activity("idle"));
    CHECK(!assistant_valid_activity("listening"));
    CHECK(!assistant_valid_activity(NULL));
    CHECK(assistant_valid_text("Couldn't reach the server · try again", ASSISTANT_DETAIL_MAX));
    CHECK(assistant_valid_text("", ASSISTANT_DETAIL_MAX));
    CHECK(!assistant_valid_text("two\nlines", ASSISTANT_DETAIL_MAX));
    CHECK(!assistant_valid_text(NULL, ASSISTANT_DETAIL_MAX));
    char longest[ASSISTANT_DETAIL_MAX + 1];
    memset(longest, 'x', ASSISTANT_DETAIL_MAX);
    longest[ASSISTANT_DETAIL_MAX] = '\0';
    CHECK(!assistant_valid_text(longest, ASSISTANT_DETAIL_MAX));
}

int main(void)
{
    test_states();
    test_activity_and_text();
    return failures ? 1 : 0;
}
