#include <stdio.h>
#include "power.h"

bool power_reboot(void)
{
    fprintf(stderr, "slate: fake reboot\n");
    return true;
}

bool power_off(void)
{
    fprintf(stderr, "slate: fake power off\n");
    return true;
}
