#include "idle.h"

#include "backlight.h"
#include "lvgl.h"
#include "prefs.h"

#define SETTING "screen_off_seconds"
#define DEFAULT_SECONDS 300
#define CHECK_MS 1000
#define RESTORE_PERCENT 80

const int idle_options[IDLE_OPTION_COUNT] = { 60, 300, 900, 0 };
const char *const idle_labels[IDLE_OPTION_COUNT] = { "1 min", "5 min", "15 min", "Never" };

typedef struct {
    int timeout;
    int saved_brightness;
    lv_obj_t *catcher;
} idle_t;

static idle_t idle;

int idle_timeout(void)
{
    return idle.timeout;
}

void idle_set_timeout(int seconds)
{
    idle.timeout = seconds;
    prefs_set_int(SETTING, seconds);
}

size_t idle_option_index(int seconds)
{
    for (size_t i = 0; i < IDLE_OPTION_COUNT; i++)
        if (idle_options[i] == seconds)
            return i;
    return 1;
}

static void wake_up(void)
{
    if (!idle.catcher)
        return;
    backlight_set(idle.saved_brightness);
    lv_obj_delete_async(idle.catcher);
    idle.catcher = NULL;
}

static void wake(lv_event_t *e)
{
    (void)e;
    wake_up();
}

static void blank(void)
{
    if (!backlight_get(&idle.saved_brightness))
        return;
    if (idle.saved_brightness < BACKLIGHT_MIN_PERCENT)
        idle.saved_brightness = RESTORE_PERCENT;
    if (!backlight_blank())
        return;
    idle.catcher = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(idle.catcher);
    lv_obj_set_size(idle.catcher, LV_PCT(100), LV_PCT(100));
    lv_obj_add_flag(idle.catcher, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(idle.catcher, wake, LV_EVENT_PRESSED, NULL);
}

static void check(lv_timer_t *timer)
{
    (void)timer;
    uint32_t inactive = lv_display_get_inactive_time(NULL);
    if (idle.catcher && inactive < CHECK_MS)
        wake_up();
    else if (!idle.catcher && idle.timeout > 0 && inactive >= (uint32_t)idle.timeout * 1000)
        blank();
}

void idle_start(void)
{
    int percent;
    if (backlight_get(&percent) && percent < BACKLIGHT_MIN_PERCENT)
        backlight_set(RESTORE_PERCENT);
    idle.timeout = prefs_get_int(SETTING, DEFAULT_SECONDS);
    lv_timer_create(check, CHECK_MS, NULL);
}
