#ifndef SLATE_CLOCK_SHEETS_H
#define SLATE_CLOCK_SHEETS_H

#include <stdint.h>

typedef void (*clock_sheet_done_cb_t)(void);

void clock_sheet_alarm_open(uint32_t id, clock_sheet_done_cb_t done);
void clock_sheet_reminder_open(uint32_t id, clock_sheet_done_cb_t done);
void clock_sheet_close(void);

#endif
