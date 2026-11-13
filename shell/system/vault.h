#ifndef SLATE_SYSTEM_VAULT_H
#define SLATE_SYSTEM_VAULT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "keystore.h"

/* Encrypted storage behind the key store: one file per key under <dir>/<namespace>/, sealed
 * with XChaCha20-Poly1305 under a device key. The namespace and key name are bound into every
 * record, so a file copied to another name or namespace no longer opens. This protects the
 * values from other apps on the device; anyone holding the whole storage card can still read
 * them, since the device key lives on it too. */

typedef void (*vault_key_cb_t)(const char *key, void *user);

bool vault_open(const char *dir);
/* 0 on success, -ENOENT when the key doesn't exist, -EIO when the record can't be opened. On
 * success *value is allocated and the caller frees it. */
int vault_get(const char *ns, const char *key, uint8_t **value, size_t *len);
/* 0 on success, -ENOSPC when the namespace already holds KEYSTORE_KEYS_MAX keys. */
int vault_set(const char *ns, const char *key, const uint8_t *value, size_t len);
/* 0 on success, -ENOENT when the key doesn't exist. */
int vault_delete(const char *ns, const char *key);
size_t vault_list(const char *ns, vault_key_cb_t cb, void *user);

#endif
