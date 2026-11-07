#include "tty.h"

#include <fcntl.h>
#include <pty.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#define FALLBACK_SHELL "/bin/sh"

static void exec_shell(void)
{
    struct passwd *pw = getpwuid(getuid());
    const char *shell = pw && pw->pw_shell && *pw->pw_shell ? pw->pw_shell : FALLBACK_SHELL;
    const char *home = pw && pw->pw_dir ? pw->pw_dir : "/";

    setenv("TERM", "xterm-256color", 1);
    setenv("COLORTERM", "truecolor", 1);
    setenv("HOME", home, 1);
    setenv("SHELL", shell, 1);
    setenv("LANG", "C.UTF-8", 0);
    if (pw)
        setenv("USER", pw->pw_name, 1);
    if (chdir(home) != 0 && chdir("/") != 0)
        _exit(126);
    signal(SIGPIPE, SIG_DFL);
    execl(shell, shell, "-l", (char *)NULL);
    _exit(127);
}

int tty_spawn(int rows, int cols, pid_t *pid)
{
    struct winsize size = { .ws_row = (unsigned short)rows, .ws_col = (unsigned short)cols };
    int fd = -1;
    pid_t child = forkpty(&fd, NULL, NULL, &size);
    if (child < 0)
        return -1;
    if (child == 0)
        exec_shell();

    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    *pid = child;
    return fd;
}

void tty_resize(int fd, int rows, int cols)
{
    struct winsize size = { .ws_row = (unsigned short)rows, .ws_col = (unsigned short)cols };
    ioctl(fd, TIOCSWINSZ, &size);
}

void tty_close(int fd, pid_t pid)
{
    if (fd >= 0)
        close(fd);
    if (pid <= 0)
        return;
    kill(-pid, SIGHUP);
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
}
