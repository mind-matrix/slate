#ifndef SLATE_CLOCK_ALERTS_H
#define SLATE_CLOCK_ALERTS_H

#include <stdbool.h>
#include <stdint.h>

#define CLOCK_KEY_ALARM "alarm"
#define CLOCK_KEY_TIMER "timer"
#define CLOCK_KEY_REMINDER "reminder"

void clock_alerts_start(void);
void clock_alerts_handled(const char *kind, uint32_t id);
bool clock_alerts_any_alarm_on(void);

#endif
