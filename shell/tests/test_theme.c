#include <math.h>

#include "check.h"
#include "theme/theme.h"

#define MIN_TEXT_CONTRAST 4.5

static void test_contrast_formula(void)
{
    CHECK(fabs(sui_contrast_ratio(0xFFFFFF, 0x000000) - 21.0) < 0.01);
    CHECK(fabs(sui_contrast_ratio(0x000000, 0xFFFFFF) - 21.0) < 0.01);
    CHECK(fabs(sui_contrast_ratio(0x777777, 0x777777) - 1.0) < 0.001);
}

static void test_text_pairs_are_readable(void)
{
    const uint32_t pairs[][2] = {
        { SUI_COLOR_TEXT, SUI_COLOR_BG },
        { SUI_COLOR_TEXT, SUI_COLOR_SURFACE },
        { SUI_COLOR_TEXT, SUI_COLOR_RAISED },
        { SUI_COLOR_TEXT_MUTED, SUI_COLOR_BG },
        { SUI_COLOR_TEXT_MUTED, SUI_COLOR_SURFACE },
        { SUI_COLOR_ON_ACCENT, SUI_COLOR_ACCENT },
        { SUI_COLOR_WHITE, SUI_COLOR_ACCENT_FILL },
        { SUI_COLOR_WHITE, SUI_COLOR_DANGER_FILL },
        { SUI_COLOR_ON_ACCENT, SUI_COLOR_WARNING_FILL },
        { SUI_COLOR_DANGER, SUI_COLOR_SURFACE },
        { SUI_COLOR_SUCCESS, SUI_COLOR_SURFACE },
    };
    for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++)
        CHECK(sui_contrast_ratio(pairs[i][0], pairs[i][1]) >= MIN_TEXT_CONTRAST);
}

static void test_type_scale_grows(void)
{
    for (int text = SUI_TEXT_CAPTION; text < SUI_TEXT_DISPLAY; text++)
        CHECK(sui_font((sui_text_t)text)->line_height < sui_font((sui_text_t)(text + 1))->line_height);
}

static void test_touch_targets(void)
{
    CHECK(SUI_TOUCH >= SUI_TOUCH_MIN);
    CHECK(SUI_ROW_H >= SUI_TOUCH_MIN);
}

int main(void)
{
    test_contrast_formula();
    test_text_pairs_are_readable();
    test_type_scale_grows();
    test_touch_targets();
    return failures ? 1 : 0;
}
