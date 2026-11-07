#include "power_screen.h"

#include <stdbool.h>
#include "components/button.h"
#include "components/dialog.h"
#include "components/header.h"
#include "components/icon.h"
#include "power.h"
#include "settings_ui.h"
#include "theme/theme.h"

#define BUTTON_W 440
#define BUTTON_H 72
#define BOTTOM_SPACE 80
#define GLYPH_SIZE 24
#define CONSEQUENCE "Running apps and Bluetooth connections will stop."

typedef struct {
    const char *label;
    const char *symbol;
    const char *question;
    const char *failure;
    uint32_t fill;
    uint32_t ink;
    bool (*run)(void);
} power_action_t;

static const power_action_t actions[] = {
    { "Restart", LV_SYMBOL_REFRESH, "Restart Slate?", "Slate couldn't restart.", SUI_COLOR_WARNING_FILL,
      SUI_COLOR_ON_ACCENT, power_reboot },
    { "Shut down", LV_SYMBOL_POWER, "Shut down Slate?", "Slate couldn't shut down.", SUI_COLOR_DANGER_FILL,
      SUI_COLOR_WHITE, power_off },
};

static lv_obj_t *dialog;

static void close_dialog(void)
{
    if (dialog)
        sui_dialog_close(dialog);
    dialog = NULL;
}

static void on_cancel(lv_event_t *e)
{
    (void)e;
    close_dialog();
}

static void on_confirm(lv_event_t *e)
{
    const power_action_t *action = lv_event_get_user_data(e);
    close_dialog();
    if (!action->run())
        settings_info_dialog("Something went wrong", action->failure);
}

static void ask(lv_event_t *e)
{
    const power_action_t *action = lv_event_get_user_data(e);
    close_dialog();
    dialog = sui_dialog_create(action->question, CONSEQUENCE);
    lv_obj_t *buttons = sui_dialog_actions(dialog);
    lv_obj_add_event_cb(sui_button_secondary_create(buttons, "Cancel"), on_cancel, LV_EVENT_CLICKED, NULL);
    lv_obj_t *confirm = sui_button_create(buttons, action->label, action->fill, action->ink);
    lv_obj_add_event_cb(confirm, on_confirm, LV_EVENT_CLICKED, (void *)action);
}

static void create(lv_obj_t *screen, void *args)
{
    (void)args;
    lv_obj_t *page = sui_page_create(screen, "Power");
    lv_obj_set_style_pad_bottom(page, BOTTOM_SPACE, 0);
    lv_obj_set_style_pad_row(page, SUI_SPACE_M, 0);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (size_t i = 0; i < sizeof(actions) / sizeof(actions[0]); i++) {
        const power_action_t *action = &actions[i];
        lv_obj_t *button = sui_button_create(page, action->label, action->fill, action->ink);
        lv_obj_set_size(button, BUTTON_W, BUTTON_H);
        lv_obj_set_style_radius(button, SUI_RADIUS_ROW, 0);
        lv_obj_set_style_text_font(button, sui_font(SUI_TEXT_ROW), 0);
        lv_obj_t *glyph = sui_glyph_create(button, SUI_SYMBOL(action->symbol), GLYPH_SIZE, action->ink);
        lv_obj_move_to_index(glyph, 0);
        lv_obj_add_event_cb(button, ask, LV_EVENT_CLICKED, (void *)action);
    }
}

static void on_hide(lv_obj_t *screen)
{
    (void)screen;
    close_dialog();
}

const sui_screen_def_t power_screen = {
    .name = "power",
    .create = create,
    .on_hide = on_hide,
};
