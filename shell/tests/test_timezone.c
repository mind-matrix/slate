#include <string.h>

#include "check.h"
#include "timezone.h"

static const char table[] =
    "# tzdb timezone descriptions\n"
    "#codes\tcoordinates\tTZ\tcomments\n"
    "IN\t+2232+08822\tAsia/Kolkata\n"
    "GB,GG,IM,JE\t+513030-0000731\tEurope/London\n"
    "AR\t-3436-05827\tAmerica/Argentina/Buenos_Aires\tBuenos Aires (BA, CF)\n"
    "US\t+404251-0740023\tAmerica/New_York\tEastern (most areas)\n"
    "\n";

static void test_parse_table(void)
{
    timezone_id_t ids[8];
    size_t count = timezone_parse_table(table, ids, 8);
    CHECK(count == 5);
    CHECK(strcmp(ids[0], TIMEZONE_UTC) == 0);
    CHECK(strcmp(ids[1], "America/Argentina/Buenos_Aires") == 0);
    CHECK(strcmp(ids[2], "America/New_York") == 0);
    CHECK(strcmp(ids[3], "Asia/Kolkata") == 0);
    CHECK(strcmp(ids[4], "Europe/London") == 0);
    CHECK(timezone_parse_table(table, ids, 2) == 2);
}

static void test_region_and_city(void)
{
    char region[32], city[32];
    CHECK(timezone_region("Asia/Kolkata", region, sizeof(region)));
    CHECK(strcmp(region, "Asia") == 0);
    CHECK(!timezone_region(TIMEZONE_UTC, region, sizeof(region)));
    CHECK(strcmp(region, "UTC") == 0);
    timezone_city("America/Argentina/Buenos_Aires", city, sizeof(city));
    CHECK(strcmp(city, "Buenos Aires") == 0);
    timezone_city("America/New_York", city, sizeof(city));
    CHECK(strcmp(city, "New York") == 0);
    timezone_city(TIMEZONE_UTC, city, sizeof(city));
    CHECK(strcmp(city, "UTC") == 0);
}

static void test_format_offset(void)
{
    char buf[32];
    timezone_format_offset(0, buf, sizeof(buf));
    CHECK(strcmp(buf, "UTC+00:00") == 0);
    timezone_format_offset(19800, buf, sizeof(buf));
    CHECK(strcmp(buf, "UTC+05:30") == 0);
    timezone_format_offset(-10800, buf, sizeof(buf));
    CHECK(strcmp(buf, "UTC\xE2\x88\x92" "03:00") == 0);
}

static void test_from_link(void)
{
    char id[TIMEZONE_ID_MAX];
    CHECK(timezone_from_link("/usr/share/zoneinfo/Asia/Kolkata", id, sizeof(id)));
    CHECK(strcmp(id, "Asia/Kolkata") == 0);
    CHECK(timezone_from_link("../usr/share/zoneinfo/Etc/UTC", id, sizeof(id)));
    CHECK(strcmp(id, TIMEZONE_UTC) == 0);
    CHECK(!timezone_from_link("/etc/localtime", id, sizeof(id)));
}

static void test_offset_lookup(void)
{
    CHECK(timezone_offset(TIMEZONE_UTC) == 0);
    CHECK(timezone_offset("Asia/Kolkata") == 19800);
}

int main(void)
{
    test_parse_table();
    test_region_and_city();
    test_format_offset();
    test_from_link();
    test_offset_lookup();
    return failures ? 1 : 0;
}
