#ifndef SLATE_PREFS_H
#define SLATE_PREFS_H

#include <stdbool.h>
#include <stddef.h>

#define PREFS_TEXT_MAX 4096

bool prefs_update(const char *text, const char *key, const char *value, char *out, size_t len);
int prefs_get_int(const char *key, int fallback);
bool prefs_set_int(const char *key, int value);
bool prefs_get_text(const char *key, char *out, size_t len);
bool prefs_set_text(const char *key, const char *value);

#endif
