#include "software_update.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "components/button.h"
#include "components/list.h"
#include "components/switch.h"
#include "diagnostics.h"
#include "power.h"
#include "prefs.h"
#include "settings_ui.h"
#include "sysinfo.h"
#include "theme/theme.h"
#include "update.h"

#define AUTO_SETTING "update_auto_check"
#define AUTO_CHECK_MS (10 * 60 * 1000)
#define REFRESH_MS 1000
#define CARD_PAD 28
#define TILE_SIZE 64
#define TILE_RADIUS 18
#define GLYPH_SIZE 30
#define BAR_H 8
#define COLUMN_GAP 24
#define TEXT 160

typedef enum {
    ACTION_NONE,
    ACTION_CHECK,
    ACTION_INSTALL,
    ACTION_CANCEL,
    ACTION_RESTART,
    ACTION_LATER,
    ACTION_DIAGNOSTICS,
} action_t;

typedef struct {
    const char *title;
    const char *detail;
    const char *symbol;
    uint32_t color;
    const char *primary;
    action_t primary_action;
    uint32_t primary_fill;
    const char *secondary;
    action_t secondary_action;
    bool progress;
} view_spec_t;

typedef struct {
    lv_obj_t *tile;
    lv_obj_t *glyph;
    lv_obj_t *title;
    lv_obj_t *detail;
    lv_obj_t *progress;
    lv_obj_t *bar;
    lv_obj_t *bar_label;
    lv_obj_t *primary;
    lv_obj_t *secondary;
    lv_obj_t *auto_card;
    lv_obj_t *auto_switch;
    lv_obj_t *notes_title;
    lv_obj_t *notes;
    lv_timer_t *timer;
    action_t primary_action;
    action_t secondary_action;
    char notes_key[UPDATE_TEXT_MAX + 4];
} update_view_t;

static update_view_t view;

typedef struct {
    const char *reason;
    view_spec_t spec;
} error_view_t;

#define RETRY .primary = "Try again", .primary_action = ACTION_CHECK, .primary_fill = SUI_COLOR_ACCENT

static const error_view_t errors[] = {
    { "offline",
      { .title = "Couldn't check for updates", .detail = "Slate is offline. Connect to Wi-Fi and try again.",
        .symbol = LV_SYMBOL_WIFI, .color = SUI_COLOR_TEXT_MUTED, RETRY } },
    { "unverified",
      { .title = "Update not installed",
        .detail = "The download couldn't be verified as a genuine Slate release, so nothing was changed.",
        .symbol = LV_SYMBOL_WARNING, .color = SUI_COLOR_DANGER, RETRY } },
    { "space",
      { .title = "Not enough space", .detail = "Free up storage to download this update.",
        .symbol = LV_SYMBOL_SD_CARD, .color = SUI_COLOR_WARNING_FILL, .primary = "Open diagnostics",
        .primary_action = ACTION_DIAGNOSTICS, .primary_fill = SUI_COLOR_ACCENT } },
    { "interrupted",
      { .title = "Update stopped",
        .detail = "The update was interrupted, so the installed version was kept. Your data is unchanged.",
        .symbol = LV_SYMBOL_REFRESH, .color = SUI_COLOR_DANGER, RETRY } },
    { "too_old",
      { .title = "Update needs a new image",
        .detail = "This version is too old to update. Flash the latest Slate image to continue.",
        .symbol = LV_SYMBOL_WARNING, .color = SUI_COLOR_WARNING_FILL } },
};

static const view_spec_t failed = {
    .title = "Update not installed",
    .detail = "Something went wrong while installing, so the installed version was kept.",
    .symbol = LV_SYMBOL_WARNING,
    .color = SUI_COLOR_DANGER,
    RETRY,
};

static bool auto_enabled(void)
{
    return prefs_get_int(AUTO_SETTING, 1) != 0;
}

static void installed_version(char *out, size_t len)
{
    char text[512], version[UPDATE_TEXT_MAX];
    if (sysinfo_read_file(SYSINFO_RELEASE_FILE, text, sizeof(text)) &&
        sysinfo_parse_key_value(text, "SLATE_VERSION", version, sizeof(version)))
        snprintf(out, len, "%s", version);
    else
        snprintf(out, len, "—");
}

static void format_checked(int64_t checked, char *out, size_t len)
{
    time_t when = (time_t)checked, now = time(NULL);
    struct tm then_tm, now_tm;
    localtime_r(&when, &then_tm);
    localtime_r(&now, &now_tm);
    if (then_tm.tm_year == now_tm.tm_year && then_tm.tm_yday == now_tm.tm_yday)
        strftime(out, len, "%H:%M", &then_tm);
    else
        strftime(out, len, "%-d %b", &then_tm);
}

void software_update_summary(char *out, size_t len, uint32_t *color)
{
    update_status_t status;
    update_read(&status);
    *color = SUI_COLOR_TEXT_MUTED;
    switch (status.state) {
    case UPDATE_DISABLED:
        snprintf(out, len, "Development build");
        break;
    case UPDATE_CURRENT:
        snprintf(out, len, "Up to date");
        break;
    case UPDATE_AVAILABLE:
        snprintf(out, len, "Update available · %s", status.version);
        *color = SUI_COLOR_ACCENT;
        break;
    case UPDATE_CHECKING:
        snprintf(out, len, "Checking…");
        break;
    case UPDATE_DOWNLOADING:
    case UPDATE_INSTALLING:
        snprintf(out, len, "Installing %s…", status.version);
        *color = SUI_COLOR_ACCENT;
        break;
    case UPDATE_RESTART:
        snprintf(out, len, "Restart to finish");
        *color = SUI_COLOR_WARNING_FILL;
        break;
    case UPDATE_ERROR:
        snprintf(out, len, "Update didn't finish");
        *color = SUI_COLOR_DANGER;
        break;
    default:
        snprintf(out, len, "Not checked yet");
        break;
    }
}

static view_spec_t spec_for(const update_status_t *status, char *detail, size_t len)
{
    char installed[UPDATE_TEXT_MAX], when[16], size[16];
    installed_version(installed, sizeof(installed));
    update_format_size(status->size, size, sizeof(size));

    switch (status->state) {
    case UPDATE_DISABLED:
        return (view_spec_t){ .title = "Development build", .detail = "Updates are off on development builds.",
                              .symbol = LV_SYMBOL_SETTINGS, .color = SUI_COLOR_TEXT_MUTED };
    case UPDATE_CURRENT:
        format_checked(status->checked, when, sizeof(when));
        snprintf(detail, len, "Version %s · last checked %s", installed, when);
        return (view_spec_t){ .title = "Slate is up to date", .detail = detail, .symbol = LV_SYMBOL_OK,
                              .color = SUI_COLOR_SUCCESS, .primary = "Check for updates",
                              .primary_action = ACTION_CHECK, .primary_fill = SUI_COLOR_ACCENT };
    case UPDATE_CHECKING:
        return (view_spec_t){ .title = "Checking for updates…", .detail = "Contacting the Slate release server",
                              .symbol = LV_SYMBOL_LOOP, .color = SUI_COLOR_ACCENT };
    case UPDATE_AVAILABLE:
        snprintf(detail, len, "Version %s · %s download", status->version, size);
        return (view_spec_t){ .title = "Update available", .detail = detail, .symbol = LV_SYMBOL_DOWNLOAD,
                              .color = SUI_COLOR_ACCENT, .primary = "Download and install",
                              .primary_action = ACTION_INSTALL, .primary_fill = SUI_COLOR_ACCENT };
    case UPDATE_DOWNLOADING:
        snprintf(detail, len, "Version %s", status->version);
        return (view_spec_t){ .title = "Downloading update", .detail = detail, .symbol = LV_SYMBOL_DOWNLOAD,
                              .color = SUI_COLOR_ACCENT, .secondary = "Cancel", .secondary_action = ACTION_CANCEL,
                              .progress = true };
    case UPDATE_INSTALLING:
        return (view_spec_t){ .title = "Installing update", .detail = "Keep Slate plugged in until this finishes.",
                              .symbol = LV_SYMBOL_LOOP, .color = SUI_COLOR_ACCENT, .progress = true };
    case UPDATE_RESTART:
        snprintf(detail, len, "Version %s is installed and starts after a restart.", status->version);
        return (view_spec_t){ .title = "Restart to finish", .detail = detail, .symbol = LV_SYMBOL_REFRESH,
                              .color = SUI_COLOR_WARNING_FILL, .primary = "Restart now",
                              .primary_action = ACTION_RESTART, .primary_fill = SUI_COLOR_WARNING_FILL,
                              .secondary = "Later", .secondary_action = ACTION_LATER };
    case UPDATE_ERROR:
        for (size_t i = 0; i < sizeof(errors) / sizeof(errors[0]); i++)
            if (strcmp(errors[i].reason, status->reason) == 0)
                return errors[i].spec;
        return failed;
    default:
        return (view_spec_t){ .title = "Software update", .detail = "Not checked yet", .symbol = LV_SYMBOL_REFRESH,
                              .color = SUI_COLOR_ACCENT, .primary = "Check for updates",
                              .primary_action = ACTION_CHECK, .primary_fill = SUI_COLOR_ACCENT };
    }
}

static void set_button(lv_obj_t *button, const char *text, uint32_t fill, uint32_t ink)
{
    settings_set_visible(button, text != NULL);
    if (!text)
        return;
    sui_button_set_text(button, text);
    lv_obj_set_style_bg_color(button, lv_color_hex(fill), 0);
    lv_obj_set_style_text_color(button, lv_color_hex(ink), 0);
}

static void show_notes(const update_status_t *status)
{
    bool available = status->state == UPDATE_AVAILABLE || status->state == UPDATE_DOWNLOADING ||
                     status->state == UPDATE_INSTALLING || status->state == UPDATE_RESTART;
    char key[sizeof(view.notes_key)], title[TEXT];
    snprintf(key, sizeof(key), "%d:%s", available, available ? status->version : "");
    if (strcmp(key, view.notes_key) == 0)
        return;
    snprintf(view.notes_key, sizeof(view.notes_key), "%s", key);

    if (available)
        snprintf(title, sizeof(title), "WHAT'S NEW IN %s", status->version);
    else
        snprintf(title, sizeof(title), "WHAT'S NEW IN THIS VERSION");
    lv_label_set_text(view.notes_title, title);

    update_note_t notes[UPDATE_MAX_NOTES];
    size_t count = update_read_notes(available, notes, UPDATE_MAX_NOTES);
    lv_obj_clean(view.notes);
    for (size_t i = 0; i < count; i++) {
        lv_obj_t *label = lv_label_create(view.notes);
        lv_obj_set_width(label, LV_PCT(100));
        lv_label_set_text_fmt(label, "•  %s", notes[i]);
        lv_obj_set_style_text_line_space(label, 4, 0);
    }
    if (!count) {
        lv_obj_t *label = lv_label_create(view.notes);
        lv_label_set_text(label, "No release notes.");
        lv_obj_set_style_text_color(label, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    }
}

static void refresh(void)
{
    update_status_t status;
    char detail[TEXT], text[TEXT];
    update_read(&status);
    view_spec_t spec = spec_for(&status, detail, sizeof(detail));

    lv_label_set_text(view.title, spec.title);
    lv_label_set_text(view.detail, spec.detail);
    lv_label_set_text(view.glyph, spec.symbol);
    lv_obj_set_style_text_color(view.glyph, lv_color_hex(spec.color), 0);
    lv_obj_set_style_bg_color(view.tile, lv_color_hex(spec.color), 0);

    settings_set_visible(view.progress, spec.progress);
    if (spec.progress) {
        bool installing = status.state == UPDATE_INSTALLING;
        lv_bar_set_value(view.bar, installing ? 100 : status.progress, LV_ANIM_OFF);
        lv_obj_set_style_bg_opa(view.bar, installing ? LV_OPA_60 : LV_OPA_COVER, LV_PART_INDICATOR);
        if (installing)
            snprintf(text, sizeof(text), "Verified and installing");
        else {
            char size[16];
            update_format_size(status.size, size, sizeof(size));
            snprintf(text, sizeof(text), "%d%% of %s", status.progress, size);
        }
        lv_label_set_text(view.bar_label, text);
    }

    set_button(view.primary, spec.primary, spec.primary_fill, SUI_COLOR_ON_ACCENT);
    set_button(view.secondary, spec.secondary, SUI_COLOR_RAISED, SUI_COLOR_TEXT);
    settings_set_visible(lv_obj_get_parent(view.primary), spec.primary || spec.secondary);
    view.primary_action = spec.primary_action;
    view.secondary_action = spec.secondary_action;

    settings_set_visible(view.auto_card, status.state != UPDATE_DISABLED);
    sui_switch_set(view.auto_switch, auto_enabled());
    show_notes(&status);
}

static void run_action(action_t action)
{
    switch (action) {
    case ACTION_CHECK:
        if (!update_start_check())
            settings_info_dialog("Something went wrong", "Slate couldn't start the update check.");
        break;
    case ACTION_INSTALL:
        if (!update_start_install())
            settings_info_dialog("Something went wrong", "Slate couldn't start the update.");
        break;
    case ACTION_CANCEL:
        update_cancel();
        break;
    case ACTION_RESTART:
        if (!power_reboot())
            settings_info_dialog("Something went wrong", "Slate couldn't restart.");
        break;
    case ACTION_LATER:
        sui_nav_pop();
        return;
    case ACTION_DIAGNOSTICS:
        sui_nav_push(&diagnostics_screen, NULL);
        return;
    default:
        return;
    }
    refresh();
}

static void on_primary(lv_event_t *e)
{
    (void)e;
    run_action(view.primary_action);
}

static void on_secondary(lv_event_t *e)
{
    (void)e;
    run_action(view.secondary_action);
}

static void toggle_auto(lv_event_t *e)
{
    (void)e;
    prefs_set_int(AUTO_SETTING, sui_switch_get(view.auto_switch) ? 1 : 0);
}

static lv_obj_t *add_column(lv_obj_t *parent)
{
    lv_obj_t *column = sui_box_create(parent);
    lv_obj_set_width(column, 0);
    lv_obj_set_flex_grow(column, 1);
    lv_obj_set_style_pad_row(column, SUI_SPACE_L - 4, 0);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    return column;
}

static void create_status_card(lv_obj_t *column)
{
    lv_obj_t *card = settings_card_create(column);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(card, CARD_PAD, 0);
    lv_obj_set_style_pad_row(card, SUI_SPACE_L - 4, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *top = sui_box_create(card);
    lv_obj_set_width(top, LV_PCT(100));
    lv_obj_set_style_pad_column(top, SUI_SPACE_L - 4, 0);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    view.tile = sui_box_create(top);
    lv_obj_set_size(view.tile, TILE_SIZE, TILE_SIZE);
    lv_obj_set_style_radius(view.tile, TILE_RADIUS, 0);
    lv_obj_set_style_bg_opa(view.tile, SUI_TINT_OPA, 0);
    view.glyph = lv_label_create(view.tile);
    lv_obj_set_style_text_font(view.glyph, sui_font(SUI_TEXT_TITLE), 0);
    lv_obj_center(view.glyph);

    lv_obj_t *text = sui_box_create(top);
    lv_obj_set_flex_grow(text, 1);
    lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(text, SUI_SPACE_XS, 0);
    view.title = lv_label_create(text);
    lv_obj_set_style_text_font(view.title, sui_font(SUI_TEXT_HEADING), 0);
    view.detail = lv_label_create(text);
    lv_obj_set_width(view.detail, LV_PCT(100));
    lv_obj_set_style_text_color(view.detail, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);

    view.progress = sui_box_create(card);
    lv_obj_set_width(view.progress, LV_PCT(100));
    lv_obj_set_style_pad_row(view.progress, SUI_SPACE_S, 0);
    lv_obj_set_flex_flow(view.progress, LV_FLEX_FLOW_COLUMN);
    view.bar = lv_bar_create(view.progress);
    lv_obj_set_size(view.bar, LV_PCT(100), BAR_H);
    lv_obj_set_style_radius(view.bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(view.bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(view.bar, lv_color_hex(SUI_COLOR_TRACK), LV_PART_MAIN);
    lv_obj_set_style_radius(view.bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(view.bar, lv_color_hex(SUI_COLOR_ACCENT), LV_PART_INDICATOR);
    view.bar_label = lv_label_create(view.progress);
    lv_obj_set_style_text_font(view.bar_label, sui_font(SUI_TEXT_CAPTION), 0);
    lv_obj_set_style_text_color(view.bar_label, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);

    lv_obj_t *buttons = sui_box_create(card);
    lv_obj_set_style_pad_column(buttons, 12, 0);
    lv_obj_set_flex_flow(buttons, LV_FLEX_FLOW_ROW);
    view.secondary = sui_button_secondary_create(buttons, "");
    lv_obj_add_event_cb(view.secondary, on_secondary, LV_EVENT_CLICKED, NULL);
    view.primary = sui_button_create(buttons, "", SUI_COLOR_ACCENT, SUI_COLOR_ON_ACCENT);
    lv_obj_add_event_cb(view.primary, on_primary, LV_EVENT_CLICKED, NULL);
}

static void add_installed_row(lv_obj_t *list, const char *title, const char *value)
{
    sui_row_t spec = { .title = title, .detail = value, .trailing = SUI_TRAILING_VALUE };
    sui_list_row_create(list, &spec);
}

static void create_details(lv_obj_t *column)
{
    lv_obj_t *section = settings_section_create(column, "", NULL);
    view.notes_title = lv_obj_get_child(lv_obj_get_child(section, 0), 0);
    view.notes = settings_card_create(section);
    lv_obj_set_style_pad_all(view.notes, SUI_SPACE_L - 4, 0);
    lv_obj_set_style_pad_row(view.notes, 10, 0);
    lv_obj_set_flex_flow(view.notes, LV_FLEX_FLOW_COLUMN);

    char version[UPDATE_TEXT_MAX];
    sysinfo_device_t device;
    installed_version(version, sizeof(version));
    sysinfo_read_device(&device);
    section = settings_section_create(column, "INSTALLED", NULL);
    lv_obj_t *list = sui_list_create(section, LV_PCT(100));
    add_installed_row(list, "Version", version);
    add_installed_row(list, "Channel", "Release");
    add_installed_row(list, "Base", device.base);
}

static void create(lv_obj_t *screen, void *args)
{
    (void)args;
    memset(&view, 0, sizeof(view));
    lv_obj_t *page = settings_page_create(screen, "Software update");
    lv_obj_t *columns = sui_box_create(page);
    lv_obj_set_width(columns, LV_PCT(100));
    lv_obj_set_style_pad_column(columns, COLUMN_GAP, 0);
    lv_obj_set_flex_flow(columns, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(columns, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *left = add_column(columns);
    create_status_card(left);
    view.auto_switch = settings_switch_card_create(left, "Check automatically",
                                                   "Once a day. Updates never install without asking.",
                                                   SUI_COLOR_CAT_SYSTEM);
    view.auto_card = lv_obj_get_parent(view.auto_switch);
    lv_obj_set_height(view.auto_card, SUI_ROW_TALL_H);
    lv_obj_add_event_cb(view.auto_switch, toggle_auto, LV_EVENT_VALUE_CHANGED, NULL);

    create_details(add_column(columns));
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    refresh();
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
    lv_timer_delete(view.timer);
    view.timer = NULL;
}

static void auto_check(lv_timer_t *timer)
{
    (void)timer;
    update_status_t status;
    update_read(&status);
    if (auto_enabled() && update_check_due(&status, (int64_t)time(NULL)))
        update_start_check();
}

void software_update_start_auto_check(void)
{
    lv_timer_create(auto_check, AUTO_CHECK_MS, NULL);
}

const sui_screen_def_t software_update_screen = {
    .name = "software_update",
    .create = create,
    .on_show = on_show,
    .on_hide = on_hide,
};
