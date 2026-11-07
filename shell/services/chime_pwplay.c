#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "audio.h"
#include "chime.h"

static pid_t player;
static chime_t playing;
static unsigned repeat;
static bool active;

static void spawn(void)
{
    char volume[16];
    snprintf(volume, sizeof(volume), "%.2f", chime_repeats(playing) ? chime_ramp(repeat) : 1.0);
    const char *role = audio_role_media(playing == CHIME_REMINDER ? AUDIO_ROLE_NOTIFICATION : AUDIO_ROLE_ALARM);
    pid_t pid = fork();
    if (pid == 0) {
        closefrom(STDERR_FILENO + 1);
        execlp("pw-play", "pw-play", "--media-role", role, "--volume", volume, chime_file(playing), (char *)NULL);
        _exit(127);
    }
    player = pid > 0 ? pid : 0;
}

void chime_start(chime_t sound)
{
    chime_stop();
    playing = sound;
    repeat = 0;
    active = true;
    spawn();
}

void chime_stop(void)
{
    active = false;
    if (player > 0) {
        kill(player, SIGTERM);
        waitpid(player, NULL, 0);
    }
    player = 0;
}

void chime_poll(void)
{
    if (player <= 0 || waitpid(player, NULL, WNOHANG) != player)
        return;
    player = 0;
    if (active && chime_repeats(playing)) {
        repeat++;
        spawn();
    } else {
        active = false;
    }
}
