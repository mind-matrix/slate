#include "components/status_bar.h"
#include "desktop.h"
#include "platform/platform.h"
#include "theme/theme.h"

int main(void)
{
    if (!sui_platform_init())
        return 1;

    sui_theme_init();
    sui_nav_init(&desktop_screen, NULL);
    sui_status_bar_create(lv_layer_top());
    sui_run();
    return 0;
}
