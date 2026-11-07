#include <unistd.h>
#include "bus.h"
#include "power.h"

#define LOGIND "org.freedesktop.login1"
#define LOGIND_PATH "/org/freedesktop/login1"
#define LOGIND_MANAGER LOGIND ".Manager"

static bool call(const char *method)
{
    sd_bus *bus = bus_system();
    sync();
    return bus && sd_bus_call_method(bus, LOGIND, LOGIND_PATH, LOGIND_MANAGER, method, NULL, NULL, "b", 0) >= 0;
}

bool power_reboot(void)
{
    return call("Reboot");
}

bool power_off(void)
{
    return call("PowerOff");
}
