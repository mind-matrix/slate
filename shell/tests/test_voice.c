#include <stdio.h>
#include <string.h>

#include "check.h"
#include "voice.h"

static voice_t make(bool required, bool running, bool reported, assistant_state_t state)
{
    voice_t v;
    memset(&v, 0, sizeof(v));
    snprintf(v.app.name, sizeof(v.app.name), "Ava");
    v.app.registration = required;
    v.running = running;
    v.reported = reported;
    v.state = state;
    return v;
}

static void test_rows(void)
{
    voice_row_t row;
    voice_t v = make(true, true, true, ASSISTANT_UNREGISTERED);
    voice_describe(&v, false, &row);
    CHECK(strcmp(row.text, "Ava needs to be set up before use") == 0 && row.action == VOICE_ACTION_SETUP);
    CHECK(row.needs_setup && !row.selectable && row.tone == VOICE_TONE_WARNING);

    v = make(true, true, true, ASSISTANT_REGISTERED);
    voice_describe(&v, true, &row);
    CHECK(strcmp(row.text, "Active · Ready") == 0 && row.tone == VOICE_TONE_SUCCESS);
    CHECK(row.action == VOICE_ACTION_LOGOUT && row.selectable);
    voice_describe(&v, false, &row);
    CHECK(strcmp(row.text, "Ready") == 0 && row.tone == VOICE_TONE_MUTED);

    v = make(false, true, false, ASSISTANT_INITIALIZING);
    voice_describe(&v, false, &row);
    CHECK(row.selectable && row.action == VOICE_ACTION_NONE);

    v = make(true, true, false, ASSISTANT_REGISTERED);
    voice_describe(&v, false, &row);
    CHECK(row.busy && strcmp(row.text, "Starting…") == 0 && !row.selectable);

    v = make(true, true, true, ASSISTANT_REGISTERING);
    voice_describe(&v, false, &row);
    CHECK(row.busy && row.action == VOICE_ACTION_NONE);

    v = make(true, true, true, ASSISTANT_ERROR);
    snprintf(v.detail, sizeof(v.detail), "Couldn't reach the Ava service");
    voice_describe(&v, false, &row);
    CHECK(strcmp(row.text, "Couldn't reach the Ava service") == 0 && row.tone == VOICE_TONE_DANGER);

    v = make(true, false, true, ASSISTANT_REGISTERED);
    voice_describe(&v, true, &row);
    CHECK(!row.available && !row.selectable && strcmp(row.text, "Not running") == 0);
}

static void test_metadata(void)
{
    char out[32];
    CHECK(voice_parse_metadata("update: id:0 key:'slate.assistant.mic' value:'ava' type:'Spa:String'",
                               VOICE_KEY_MIC, out, sizeof(out)) &&
          strcmp(out, "ava") == 0);
    CHECK(voice_parse_metadata("update: id:0 key:'slate.assistant.mic' value:'' type:'Spa:String'", VOICE_KEY_MIC,
                               out, sizeof(out)) &&
          out[0] == '\0');
    CHECK(!voice_parse_metadata("update: id:0 key:'slate.assistant.active' value:'ava' type:''", VOICE_KEY_MIC, out,
                                sizeof(out)));
    CHECK(!voice_parse_metadata("update: id:42 key:'slate.assistant.mic' value:'ava'", VOICE_KEY_MIC, out,
                                sizeof(out)));
}

int main(void)
{
    test_rows();
    test_metadata();
    return failures ? 1 : 0;
}
