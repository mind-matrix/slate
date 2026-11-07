#ifndef SLATE_BLUETOOTH_H
#define SLATE_BLUETOOTH_H

#include <stddef.h>
#include "nav/nav.h"

extern const sui_screen_def_t bluetooth_screen;

void bluetooth_agent_init(void);
void bluetooth_summary(char *out, size_t len);

#endif
