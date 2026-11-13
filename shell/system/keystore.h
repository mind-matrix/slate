#ifndef SLATE_SYSTEM_KEYSTORE_H
#define SLATE_SYSTEM_KEYSTORE_H

#include <stdbool.h>
#include <stddef.h>

/* Slate's key store keeps small secrets for the shell and for apps granted "secrets". Each
 * caller has its own namespace, taken from the user it runs as, so no caller can name another
 * caller's keys. */

#define KEYSTORE_KEY_MAX 128
#define KEYSTORE_VALUE_MAX (64 * 1024)
#define KEYSTORE_KEYS_MAX 1024
#define KEYSTORE_NAMESPACE_MAX 40
#define KEYSTORE_SHELL_NAMESPACE "shell"
#define KEYSTORE_SHELL_USER "slate"

/* Keys are 1-128 characters of letters, digits, '.', '_' and '-', not starting with '.'. */
bool keystore_valid_key(const char *key);

/* The namespace for a caller's user: "slate" is the shell, "app-<id>" is that app. Every
 * other user has none. */
bool keystore_namespace(const char *user, char *out, size_t len);

#endif
