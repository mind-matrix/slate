#include "voice_assistant.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "apps.h"
#include "components/app_logo.h"
#include "components/button.h"
#include "components/status_bar.h"
#include "notify.h"
#include "platform/platform.h"
#include "platform/windows.h"
#include "settings_ui.h"
#include "theme/theme.h"
#include "voice.h"

#define CARD_W 880
#define ROW_H 96
#define LOGO 56
#define RING 28
#define DOT 14
#define BUTTON_H 48
#define SPINNER 16
#define RING_ENABLED 0x6A769A
#define REFRESH_MS 1000
#define TOKEN_MAX 64
#define MONITOR_LINE_MAX 512
#define SIGNATURE_MAX 2048

typedef struct {
    char id[APPS_ID_MAX];
    voice_action_t action;
    bool selectable;
    bool needs_setup;
} pick_t;

static struct {
    lv_obj_t *list;
    lv_timer_t *timer;
    pick_t picks[VOICE_MAX + 1];
    char signature[SIGNATURE_MAX];
} view;

static struct {
    char pending[APPS_ID_MAX];
    char mic[APPS_ID_MAX];
    char line[MONITOR_LINE_MAX];
    size_t line_len;
    int monitor;
} state = { .monitor = -1 };

static void refresh(void);

/* ---- shared state ------------------------------------------------------------------------ */

static bool find(const char *id, voice_t *out)
{
    voice_t voices[VOICE_MAX];
    size_t count = voice_list(voices, VOICE_MAX);
    for (size_t i = 0; i < count; i++)
        if (strcmp(voices[i].app.id, id) == 0) {
            *out = voices[i];
            return true;
        }
    return false;
}

static void open_screen(void)
{
    sui_nav_push(&voice_assistant_screen, NULL);
}

static void update_badge(void)
{
    voice_t voice;
    if (state.mic[0] && find(state.mic, &voice))
        sui_status_bar_set_mic(voice.app.name, voice.capturing, open_screen);
    else
        sui_status_bar_set_mic(NULL, false, NULL);
}

static void request(const char *id, voice_action_t action)
{
    char token[TOKEN_MAX] = "";
    sui_windows_token(id, token, sizeof(token));
    if (!voice_request(id, action, token))
        settings_info_dialog("Something went wrong", "Slate couldn't reach the assistant.");
}

static void on_changed(void *user)
{
    (void)user;
    voice_t voice;
    /* An assistant picked before it was set up becomes active once it reports ready. */
    if (state.pending[0] && find(state.pending, &voice) && voice_state(&voice) == ASSISTANT_REGISTERED) {
        voice_set_active(state.pending);
        state.pending[0] = '\0';
    }
    voice_sync_audio();
    update_badge();
    if (view.timer)
        refresh();
}

static void on_requested(const char *id, const char *reason, void *user)
{
    (void)user;
    voice_t voice;
    char title[NOTIFY_TITLE_MAX + APPS_NAME_MAX];
    snprintf(title, sizeof(title), "%s is now your voice assistant", find(id, &voice) ? voice.app.name : id);
    notify_post(NOTIFY_ASSISTANT, NOTIFY_ICON_ASSISTANT, "active", title, reason);
}

static void read_monitor(int fd, void *user)
{
    (void)user;
    char buf[256];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        for (ssize_t i = 0; i < n; i++) {
            if (buf[i] != '\n') {
                if (state.line_len < sizeof(state.line) - 1)
                    state.line[state.line_len++] = buf[i];
                continue;
            }
            state.line[state.line_len] = '\0';
            state.line_len = 0;
            if (voice_parse_metadata(state.line, VOICE_KEY_MIC, state.mic, sizeof(state.mic)))
                update_badge();
        }
    if (n == 0) {
        sui_unwatch_fd(fd);
        state.monitor = -1;
    }
}

static void keep_monitor(lv_timer_t *timer)
{
    (void)timer;
    if (state.monitor < 0 && (state.monitor = voice_mic_monitor()) >= 0)
        sui_watch_fd(state.monitor, read_monitor, NULL);
}

void voice_assistant_start(void)
{
    voice_watch(on_changed, on_requested, NULL);
    voice_sync_audio();
    keep_monitor(lv_timer_create(keep_monitor, 5000, NULL));
}

void voice_assistant_summary(char *out, size_t len)
{
    char active[APPS_ID_MAX] = "";
    voice_t voice;
    voice_get_active(active, sizeof(active));
    snprintf(out, len, "%s", active[0] && find(active, &voice) ? voice.app.name : "No assistant");
}

/* ---- screen ------------------------------------------------------------------------------ */

static uint32_t tone_color(voice_tone_t tone)
{
    switch (tone) {
    case VOICE_TONE_SUCCESS:
        return SUI_COLOR_SUCCESS;
    case VOICE_TONE_WARNING:
        return SUI_COLOR_WARNING_FILL;
    case VOICE_TONE_DANGER:
        return SUI_COLOR_DANGER;
    default:
        return SUI_COLOR_TEXT_MUTED;
    }
}

static void on_pick(lv_event_t *e)
{
    const pick_t *pick = lv_event_get_user_data(e);
    if (pick->selectable || !pick->id[0]) {
        state.pending[0] = '\0';
        voice_set_active(pick->id);
    } else if (pick->needs_setup) {
        snprintf(state.pending, sizeof(state.pending), "%s", pick->id);
        request(pick->id, VOICE_ACTION_SETUP);
    }
    on_changed(NULL);
}

static void on_action(lv_event_t *e)
{
    const pick_t *pick = lv_event_get_user_data(e);
    char active[APPS_ID_MAX] = "";
    voice_get_active(active, sizeof(active));
    if (pick->action == VOICE_ACTION_LOGOUT && strcmp(active, pick->id) == 0)
        voice_set_active("");
    request(pick->id, pick->action);
    on_changed(NULL);
}

static void add_row(size_t index, const char *name, const char *logo, const voice_row_t *row, bool selected,
                    pick_t *pick)
{
    lv_obj_t *item = sui_box_create(view.list);
    lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(item, LV_PCT(100), ROW_H);
    lv_obj_set_style_pad_left(item, SUI_SPACE_L, 0);
    lv_obj_set_style_pad_right(item, SUI_SPACE_L, 0);
    lv_obj_set_style_pad_column(item, 20, 0);
    lv_obj_set_style_opa(item, row->available ? LV_OPA_COVER : LV_OPA_50, 0);
    if (index > 0) {
        lv_obj_set_style_border_side(item, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_width(item, 1, 0);
        lv_obj_set_style_border_color(item, lv_color_hex(SUI_COLOR_BORDER), 0);
    }
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(item, on_pick, LV_EVENT_CLICKED, pick);

    if (logo)
        sui_app_logo_create(item, logo, name, LOGO);
    else {
        lv_obj_t *none = sui_box_create(item);
        lv_obj_set_size(none, LOGO, LOGO);
        lv_obj_set_style_radius(none, 16, 0);
        lv_obj_set_style_bg_color(none, lv_color_hex(SUI_COLOR_RAISED), 0);
        lv_obj_set_style_bg_opa(none, LV_OPA_COVER, 0);
        lv_obj_t *dash = lv_label_create(none);
        lv_label_set_text(dash, "–");
        lv_obj_set_style_text_font(dash, sui_font(SUI_TEXT_HEADING), 0);
        lv_obj_set_style_text_color(dash, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
        lv_obj_center(dash);
    }

    lv_obj_t *texts = sui_box_create(item);
    lv_obj_set_width(texts, 0);
    lv_obj_set_flex_grow(texts, 1);
    lv_obj_set_flex_flow(texts, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(texts, 3, 0);
    lv_obj_t *title = lv_label_create(texts);
    lv_label_set_text(title, name);
    lv_obj_set_style_text_font(title, sui_font(SUI_TEXT_ROW), 0);
    lv_obj_t *line = sui_box_create(texts);
    lv_obj_set_flex_flow(line, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(line, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(line, SUI_SPACE_S, 0);
    if (row->busy) {
        lv_obj_t *spinner = lv_spinner_create(line);
        lv_obj_set_size(spinner, SPINNER, SPINNER);
        lv_obj_set_style_arc_width(spinner, 2, LV_PART_MAIN);
        lv_obj_set_style_arc_width(spinner, 2, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(spinner, lv_color_hex(SUI_COLOR_TRACK), LV_PART_MAIN);
        lv_obj_set_style_arc_color(spinner, lv_color_hex(SUI_COLOR_CAT_BLUETOOTH), LV_PART_INDICATOR);
    }
    lv_obj_t *detail = lv_label_create(line);
    lv_label_set_text(detail, row->text);
    lv_obj_set_style_text_font(detail, sui_font(SUI_TEXT_CAPTION), 0);
    lv_obj_set_style_text_color(detail, lv_color_hex(tone_color(row->tone)), 0);

    if (row->action != VOICE_ACTION_NONE) {
        bool logout = row->action == VOICE_ACTION_LOGOUT;
        lv_obj_t *button = sui_button_secondary_create(item, logout ? "Logout" : "Setup");
        lv_obj_set_height(button, BUTTON_H);
        if (logout)
            lv_obj_set_style_text_color(button, lv_color_hex(SUI_COLOR_DANGER), 0);
        lv_obj_add_event_cb(button, on_action, LV_EVENT_CLICKED, pick);
    }

    bool enabled = row->selectable || row->needs_setup || !pick->id[0];
    lv_obj_t *ring = sui_box_create(item);
    lv_obj_set_size(ring, RING, RING);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_set_style_border_color(
        ring, lv_color_hex(selected ? SUI_COLOR_CAT_BLUETOOTH : enabled ? RING_ENABLED : SUI_COLOR_TRACK), 0);
    if (selected) {
        lv_obj_t *dot = sui_box_create(ring);
        lv_obj_set_size(dot, DOT, DOT);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(SUI_COLOR_CAT_BLUETOOTH), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_center(dot);
    }
}

static void refresh(void)
{
    voice_t voices[VOICE_MAX];
    voice_row_t rows[VOICE_MAX];
    size_t count = voice_list(voices, VOICE_MAX);
    char active[APPS_ID_MAX] = "", signature[SIGNATURE_MAX];
    voice_get_active(active, sizeof(active));
    size_t used = (size_t)snprintf(signature, sizeof(signature), "%s|", active);
    for (size_t i = 0; i < count; i++) {
        voice_describe(&voices[i], strcmp(active, voices[i].app.id) == 0, &rows[i]);
        if (used < sizeof(signature))
            used += (size_t)snprintf(signature + used, sizeof(signature) - used, "%s:%s:%d:%d|", voices[i].app.id,
                                     rows[i].text, rows[i].action, rows[i].busy);
    }
    if (strcmp(signature, view.signature) == 0)
        return;
    snprintf(view.signature, sizeof(view.signature), "%s", signature);

    lv_obj_clean(view.list);
    for (size_t i = 0; i < count; i++) {
        pick_t *pick = &view.picks[i];
        memcpy(pick->id, voices[i].app.id, sizeof(pick->id));
        pick->action = rows[i].action;
        pick->selectable = rows[i].selectable;
        pick->needs_setup = rows[i].needs_setup && !rows[i].busy;
        add_row(i, voices[i].app.name, voices[i].app.logo, &rows[i], strcmp(active, pick->id) == 0, pick);
    }
    pick_t *none = &view.picks[count];
    memset(none, 0, sizeof(*none));
    voice_row_t off = { .available = true, .tone = VOICE_TONE_MUTED };
    snprintf(off.text, sizeof(off.text), "Voice Assistants are disabled");
    add_row(count, "No assistant", NULL, &off, active[0] == '\0', none);
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    refresh();
}

static void create(lv_obj_t *screen, void *args)
{
    (void)args;
    lv_obj_t *page = settings_page_create(screen, "Voice assistant");
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    view.list = settings_card_create(page);
    lv_obj_set_size(view.list, CARD_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(view.list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_clip_corner(view.list, true, 0);
    lv_obj_t *note = lv_label_create(page);
    lv_label_set_text(note, "Only the selected assistant can use the microphone and speaker.");
    lv_obj_set_width(note, CARD_W);
    lv_obj_set_style_pad_left(note, SUI_SPACE_M, 0);
    lv_obj_set_style_text_font(note, sui_font(SUI_TEXT_CAPTION), 0);
    lv_obj_set_style_text_color(note, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    view.signature[0] = '\0';
}

static void on_show(lv_obj_t *screen)
{
    (void)screen;
    refresh();
    view.timer = lv_timer_create(tick, REFRESH_MS, NULL);
}

static void on_hide(lv_obj_t *screen)
{
    (void)screen;
    if (view.timer)
        lv_timer_delete(view.timer);
    view.timer = NULL;
}

const sui_screen_def_t voice_assistant_screen = {
    .name = "voice_assistant",
    .create = create,
    .on_show = on_show,
    .on_hide = on_hide,
};
