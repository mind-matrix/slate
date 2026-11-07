#ifndef SLATE_TTY_H
#define SLATE_TTY_H

#include <sys/types.h>

int tty_spawn(int rows, int cols, pid_t *pid);
void tty_resize(int fd, int rows, int cols);
void tty_close(int fd, pid_t pid);

#endif
