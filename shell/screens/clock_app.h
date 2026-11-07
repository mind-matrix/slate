#ifndef SLATE_CLOCK_APP_H
#define SLATE_CLOCK_APP_H

#include <stddef.h>
#include "nav/nav.h"

extern const sui_screen_def_t clock_app_screen;

void clock_app_refresh(void);
void clock_app_select(size_t tab);

#endif
