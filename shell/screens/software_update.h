#ifndef SLATE_SOFTWARE_UPDATE_H
#define SLATE_SOFTWARE_UPDATE_H

#include <stddef.h>
#include <stdint.h>
#include "nav/nav.h"

#define SOFTWARE_UPDATE_SUMMARY 64

extern const sui_screen_def_t software_update_screen;

void software_update_summary(char *out, size_t len, uint32_t *color);
void software_update_start_auto_check(void);

#endif
