#ifndef SLATE_IDLE_H
#define SLATE_IDLE_H

#include <stddef.h>

#define IDLE_OPTION_COUNT 4

extern const int idle_options[IDLE_OPTION_COUNT];
extern const char *const idle_labels[IDLE_OPTION_COUNT];

void idle_start(void);
int idle_timeout(void);
void idle_set_timeout(int seconds);
size_t idle_option_index(int seconds);

#endif
