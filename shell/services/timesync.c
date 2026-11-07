#include <unistd.h>

#include "clock.h"

bool clock_time_synced(void)
{
    return access(CLOCK_SYNC_FILE, F_OK) == 0;
}
