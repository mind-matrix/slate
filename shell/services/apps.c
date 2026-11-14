#include "apps.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "sysinfo.h"

bool apps_valid_id(const char *id)
{
    if (!id || !(islower((unsigned char)id[0]) || isdigit((unsigned char)id[0])))
        return false;
    size_t n = 0;
    for (; id[n]; n++)
        if (!(islower((unsigned char)id[n]) || isdigit((unsigned char)id[n]) || id[n] == '-'))
            return false;
    return n < APPS_ID_MAX;
}

void apps_parse_app(const char *text, const char *id, app_t *out)
{
    char registration[16] = "";
    memset(out, 0, sizeof(*out));
    snprintf(out->id, sizeof(out->id), "%s", id);
    if (!sysinfo_parse_key_value(text, "name", out->name, sizeof(out->name)) || !out->name[0])
        snprintf(out->name, sizeof(out->name), "%s", id);
    sysinfo_parse_key_value(text, "permissions", out->requested, sizeof(out->requested));
    sysinfo_parse_key_value(text, "registration", registration, sizeof(registration));
    out->registration = strcmp(registration, "required") == 0;
}

bool apps_has(const char *list, const char *word)
{
    size_t n = strlen(word);
    for (const char *p = list; n && (p = strstr(p, word)); p += n)
        if ((p == list || p[-1] == ' ') && (p[n] == '\0' || p[n] == ' ' || p[n] == '\n'))
            return true;
    return false;
}

size_t apps_count(const char *list)
{
    size_t count = 0;
    bool in_word = false;
    for (const char *p = list; *p; p++) {
        bool space = isspace((unsigned char)*p);
        count += !space && !in_word;
        in_word = !space;
    }
    return count;
}

static int by_name(const void *a, const void *b)
{
    return strcasecmp(((const app_t *)a)->name, ((const app_t *)b)->name);
}

void apps_sort(app_t *apps, size_t count)
{
    qsort(apps, count, sizeof(app_t), by_name);
}

bool apps_always_on(const app_t *app)
{
    return apps_has(app->granted, "assistant");
}
