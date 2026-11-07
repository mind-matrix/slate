#include "bluetooth.h"

#include <stdio.h>
#include <string.h>
#include "bt.h"
#include "components/button.h"
#include "components/dialog.h"
#include "components/list.h"
#include "components/switch.h"
#include "settings_ui.h"
#include "theme/theme.h"

#define REFRESH_MS 2000
#define SIGNATURE_MAX 2048
#define TEXT 96
#define CODE_LETTER_SPACE 6

typedef struct {
    lv_obj_t *toggle;
    lv_obj_t *visible;
    lv_obj_t *mine_section;
    lv_obj_t *mine;
    lv_obj_t *others_section;
    lv_obj_t *scanning;
    lv_obj_t *others;
    lv_obj_t *empty_off;
    lv_obj_t *empty_missing;
    lv_timer_t *timer;
    bt_device_t devices[BT_MAX_DEVICES];
    size_t count;
    char signature[SIGNATURE_MAX];
    bool shown;
} bluetooth_view_t;

static bluetooth_view_t view;
static lv_obj_t *pair_dialog;

void bluetooth_summary(char *out, size_t len)
{
    char name[BT_NAME_MAX];
    if (!bt_powered())
        snprintf(out, len, "Off");
    else if (bt_connected_name(name, sizeof(name)))
        snprintf(out, len, "%s connected", name);
    else
        snprintf(out, len, "On");
}

static const bt_device_t *device_at(lv_event_t *e)
{
    size_t index = (size_t)(uintptr_t)lv_event_get_user_data(e);
    return index < view.count ? &view.devices[index] : NULL;
}

static void force_refresh(void)
{
    view.signature[0] = '\0';
}

static void on_connect(lv_event_t *e)
{
    const bt_device_t *device = device_at(e);
    if (!device)
        return;
    if (device->connected)
        bt_disconnect(device->path);
    else
        bt_connect(device->path);
    force_refresh();
}

static void on_forget(lv_event_t *e)
{
    const bt_device_t *device = device_at(e);
    if (device)
        bt_forget(device->path);
    force_refresh();
}

static void on_pair(lv_event_t *e)
{
    const bt_device_t *device = device_at(e);
    if (device && bt_activity(device->path) == BT_IDLE)
        bt_pair(device->path);
    force_refresh();
}

static void describe(const bt_device_t *device, char *detail, size_t len, uint32_t *color)
{
    const char *kind = bt_kind_label(device->icon);
    *color = SUI_COLOR_TEXT_MUTED;
    switch (bt_activity(device->path)) {
    case BT_PAIRING:
        snprintf(detail, len, "Pairing…");
        return;
    case BT_CONNECTING:
        snprintf(detail, len, "Connecting…");
        return;
    case BT_DISCONNECTING:
        snprintf(detail, len, "Disconnecting…");
        return;
    case BT_FAILED:
        snprintf(detail, len, "Couldn't connect · %s", kind);
        *color = SUI_COLOR_DANGER;
        return;
    default:
        break;
    }
    if (device->connected) {
        snprintf(detail, len, "Connected · %s", kind);
        *color = SUI_COLOR_SUCCESS;
    } else
        snprintf(detail, len, "%s", kind);
}

static lv_obj_t *add_device_row(lv_obj_t *list, size_t index, bool mine)
{
    const bt_device_t *device = &view.devices[index];
    char detail[TEXT];
    uint32_t color;
    describe(device, detail, sizeof(detail), &color);

    sui_row_t spec = { SUI_SYMBOL(LV_SYMBOL_BLUETOOTH), mine ? SUI_COLOR_CAT_BLUETOOTH : SUI_COLOR_TEXT_MUTED,
                       device->name, detail, SUI_TRAILING_NONE };
    lv_obj_t *row = sui_list_row_create(list, &spec);
    sui_list_row_set_detail_color(row, color);
    void *user = (void *)(uintptr_t)index;
    bool busy = bt_activity(device->path) != BT_IDLE && bt_activity(device->path) != BT_FAILED;

    if (mine) {
        lv_obj_t *connect = sui_button_secondary_create(row, device->connected ? "Disconnect" : "Connect");
        lv_obj_add_event_cb(connect, on_connect, LV_EVENT_CLICKED, user);
        lv_obj_t *forget = sui_button_create(row, "Forget", SUI_COLOR_RAISED, SUI_COLOR_DANGER);
        lv_obj_add_event_cb(forget, on_forget, LV_EVENT_CLICKED, user);
        if (busy)
            lv_obj_add_state(connect, LV_STATE_DISABLED);
        return row;
    }

    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(row, lv_color_hex(SUI_COLOR_RAISED), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_add_event_cb(row, on_pair, LV_EVENT_CLICKED, user);
    lv_obj_t *pair = lv_label_create(row);
    lv_label_set_text(pair, busy ? "" : "Pair");
    lv_obj_set_style_text_color(pair, lv_color_hex(SUI_COLOR_CAT_BLUETOOTH), 0);
    return row;
}

static void add_placeholder(lv_obj_t *list, const char *text)
{
    sui_row_t spec = { .title = text, .trailing = SUI_TRAILING_NONE };
    lv_obj_t *row = sui_list_row_create(list, &spec);
    lv_obj_set_style_text_color(lv_obj_get_child(lv_obj_get_child(row, 0), 0), lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
}

static void show_devices(void)
{
    char signature[SIGNATURE_MAX] = "";
    size_t used = 0;
    view.count = bt_list(view.devices, BT_MAX_DEVICES);
    for (size_t i = 0; i < view.count; i++) {
        const bt_device_t *d = &view.devices[i];
        int n = snprintf(signature + used, sizeof(signature) - used, "%s|%s|%d%d%d;", d->path, d->name, d->paired,
                         d->connected, (int)bt_activity(d->path));
        if (n < 0 || (size_t)n >= sizeof(signature) - used)
            break;
        used += (size_t)n;
    }
    if (strcmp(signature, view.signature) == 0)
        return;
    snprintf(view.signature, sizeof(view.signature), "%s", signature);

    lv_obj_clean(view.mine);
    lv_obj_clean(view.others);
    size_t mine = 0, others = 0;
    for (size_t i = 0; i < view.count; i++) {
        bool paired = view.devices[i].paired;
        add_device_row(paired ? view.mine : view.others, i, paired);
        if (paired)
            mine++;
        else
            others++;
    }
    if (!mine)
        add_placeholder(view.mine, "No paired devices");
    if (!others)
        add_placeholder(view.others, "Looking for devices…");
}

static void refresh(void)
{
    bool available = bt_available();
    bool on = available && bt_powered();
    char alias[BT_NAME_MAX], visible[TEXT];

    sui_switch_set(view.toggle, on);
    if (bt_alias(alias, sizeof(alias)))
        snprintf(visible, sizeof(visible), on ? "Visible as “%s”" : "Off", alias);
    else
        snprintf(visible, sizeof(visible), on ? "On" : "Off");
    lv_label_set_text(view.visible, visible);

    settings_set_visible(lv_obj_get_parent(view.toggle), available);
    settings_set_visible(view.empty_missing, !available);
    settings_set_visible(view.empty_off, available && !on);
    settings_set_visible(view.mine_section, on);
    settings_set_visible(view.others_section, on);
    lv_label_set_text(view.scanning, on ? "Scanning…" : "");
    if (on)
        show_devices();
    else
        force_refresh();
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    refresh();
}

static void toggle(lv_event_t *e)
{
    (void)e;
    bool on = sui_switch_get(view.toggle);
    bt_set_powered(on);
    if (on && view.shown)
        bt_set_discovering(true);
    refresh();
}

static void create(lv_obj_t *screen, void *args)
{
    (void)args;
    memset(&view, 0, sizeof(view));
    lv_obj_t *page = settings_page_create(screen, "Bluetooth");
    view.toggle = settings_switch_card_create(page, "Bluetooth", "", SUI_COLOR_CAT_BLUETOOTH);
    view.visible = lv_obj_get_child(lv_obj_get_child(lv_obj_get_parent(view.toggle), 0), 1);
    lv_obj_add_event_cb(view.toggle, toggle, LV_EVENT_VALUE_CHANGED, NULL);

    view.mine_section = settings_section_create(page, "MY DEVICES", NULL);
    view.mine = sui_list_create(view.mine_section, LV_PCT(100));
    view.others_section = settings_section_create(page, "OTHER DEVICES", &view.scanning);
    view.others = sui_list_create(view.others_section, LV_PCT(100));
    view.empty_off = settings_empty_state_create(page, SUI_SYMBOL(LV_SYMBOL_BLUETOOTH), "Bluetooth is off",
                                                 "Turn it on to pair and connect devices.");
    view.empty_missing = settings_empty_state_create(page, SUI_SYMBOL(LV_SYMBOL_BLUETOOTH), "No Bluetooth",
                                                     "This device has no Bluetooth adapter.");
}

static void on_show(lv_obj_t *screen)
{
    (void)screen;
    view.shown = true;
    if (bt_powered())
        bt_set_discovering(true);
    refresh();
    view.timer = lv_timer_create(tick, REFRESH_MS, NULL);
}

static void on_hide(lv_obj_t *screen)
{
    (void)screen;
    view.shown = false;
    bt_set_discovering(false);
    lv_timer_delete(view.timer);
    view.timer = NULL;
}

static void answer(lv_event_t *e)
{
    bt_agent_reply(lv_event_get_user_data(e) != NULL);
    if (pair_dialog)
        sui_dialog_close(pair_dialog);
    pair_dialog = NULL;
}

static void confirm_pairing(const char *device, uint32_t passkey, void *user)
{
    (void)user;
    if (pair_dialog)
        sui_dialog_close(pair_dialog);
    pair_dialog = NULL;
    if (!device)
        return;

    char name[BT_NAME_MAX] = "this device", title[TEXT + BT_NAME_MAX], code[16];
    for (size_t i = 0; i < view.count; i++)
        if (strcmp(view.devices[i].path, device) == 0)
            snprintf(name, sizeof(name), "%s", view.devices[i].name);
    snprintf(title, sizeof(title), "Pair with %s?", name);
    bt_format_passkey(passkey, code, sizeof(code));

    pair_dialog = sui_dialog_create(title, "Check that this code is shown on the device.");
    lv_obj_t *label = lv_label_create(pair_dialog);
    lv_label_set_text(label, code);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, sui_font(SUI_TEXT_DISPLAY), 0);
    lv_obj_set_style_text_letter_space(label, CODE_LETTER_SPACE, 0);
    lv_obj_move_to_index(label, 2);

    lv_obj_t *actions = sui_dialog_actions(pair_dialog);
    lv_obj_add_event_cb(sui_button_secondary_create(actions, "Cancel"), answer, LV_EVENT_CLICKED, NULL);
    lv_obj_t *pair = sui_button_create(actions, "Pair", SUI_COLOR_CAT_BLUETOOTH, SUI_COLOR_ON_ACCENT);
    lv_obj_add_event_cb(pair, answer, LV_EVENT_CLICKED, (void *)1);
}

void bluetooth_agent_init(void)
{
    bt_agent_start(confirm_pairing, NULL);
}

const sui_screen_def_t bluetooth_screen = {
    .name = "bluetooth",
    .create = create,
    .on_show = on_show,
    .on_hide = on_hide,
};
