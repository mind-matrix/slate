#include "app_stub.h"

#include "components/header.h"
#include "theme/theme.h"

static void create(lv_obj_t *screen, void *args)
{
    sui_header_create(screen, args);

    lv_obj_t *note = lv_label_create(screen);
    lv_label_set_text(note, "Coming soon");
    lv_obj_set_style_text_font(note, sui_font(SUI_TEXT_ROW), 0);
    lv_obj_set_style_text_color(note, lv_color_hex(SUI_COLOR_TEXT_MUTED), 0);
    lv_obj_center(note);
}

const sui_screen_def_t app_stub_screen = {
    .name = "app_stub",
    .create = create,
};
