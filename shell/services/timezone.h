#ifndef SLATE_TIMEZONE_H
#define SLATE_TIMEZONE_H

#include <stdbool.h>
#include <stddef.h>

#define TIMEZONE_ID_MAX 64
#define TIMEZONE_UTC "Etc/UTC"

typedef char timezone_id_t[TIMEZONE_ID_MAX];

size_t timezone_parse_table(const char *text, timezone_id_t *ids, size_t max);
bool timezone_region(const char *id, char *region, size_t len);
void timezone_city(const char *id, char *city, size_t len);
void timezone_format_offset(long seconds, char *buf, size_t len);
bool timezone_from_link(const char *target, char *id, size_t len);

size_t timezone_load_all(timezone_id_t *ids, size_t max);
bool timezone_current(char *id, size_t len);
long timezone_offset(const char *id);
bool timezone_set(const char *id);
bool timezone_ntp_synced(void);

#endif
