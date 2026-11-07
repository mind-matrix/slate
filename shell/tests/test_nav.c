#include <string.h>

#include "check.h"
#include "nav/nav.h"
#include "platform/platform.h"
#include "theme/theme.h"

static char events[512];

static void record(const char *event)
{
    strncat(events, event, sizeof(events) - strlen(events) - 1);
}

#define SCREEN(id)                                                     \
    static void create_##id(lv_obj_t *s, void *a) { (void)s; (void)a; record("C" #id " "); } \
    static void show_##id(lv_obj_t *s) { (void)s; record("S" #id " "); } \
    static void hide_##id(lv_obj_t *s) { (void)s; record("H" #id " "); } \
    static const sui_screen_def_t screen_##id = { #id, create_##id, show_##id, hide_##id };

SCREEN(a)
SCREEN(b)
SCREEN(c)

static bool events_are(const char *expected)
{
    bool same = strcmp(events, expected) == 0;
    if (!same)
        fprintf(stderr, "events: \"%s\" expected \"%s\"\n", events, expected);
    events[0] = '\0';
    return same;
}

static void test_init_shows_home(void)
{
    sui_nav_init(&screen_a, NULL);
    CHECK(events_are("Ca Sa "));
    CHECK(sui_nav_depth() == 1);
    CHECK(sui_nav_top() == &screen_a);
}

static void test_push_hides_then_shows(void)
{
    sui_nav_push(&screen_b, NULL);
    CHECK(events_are("Ha Cb Sb "));
    sui_nav_push(&screen_c, NULL);
    CHECK(events_are("Hb Cc Sc "));
    CHECK(sui_nav_depth() == 3);
    CHECK(sui_nav_top() == &screen_c);
}

static void test_pop_returns_to_previous(void)
{
    sui_nav_pop();
    CHECK(events_are("Hc Sb "));
    CHECK(sui_nav_depth() == 2);
    CHECK(sui_nav_top() == &screen_b);
}

static void test_home_skips_intermediate_screens(void)
{
    sui_nav_push(&screen_c, NULL);
    events[0] = '\0';
    sui_nav_home();
    CHECK(events_are("Hc Sa "));
    CHECK(sui_nav_depth() == 1);
}

static void test_root_cannot_be_popped(void)
{
    sui_nav_pop();
    sui_nav_home();
    CHECK(events_are(""));
    CHECK(sui_nav_depth() == 1);
}

static void test_stack_is_bounded(void)
{
    for (int i = 0; i < SUI_NAV_MAX + 3; i++)
        sui_nav_push(&screen_b, NULL);
    CHECK(sui_nav_depth() == SUI_NAV_MAX);
    sui_nav_home();
    lv_timer_handler();
    CHECK(sui_nav_depth() == 1);
}

int main(void)
{
    sui_headless_init();
    sui_theme_init();

    test_init_shows_home();
    test_push_hides_then_shows();
    test_pop_returns_to_previous();
    test_home_skips_intermediate_screens();
    test_root_cannot_be_popped();
    test_stack_is_bounded();
    return failures ? 1 : 0;
}
