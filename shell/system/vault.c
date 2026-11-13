#include "vault.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEVICE_KEY "device.key"
#define MAGIC "SKV1"
#define MAGIC_LEN 4
#define NONCE_LEN crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
#define TAG_LEN crypto_aead_xchacha20poly1305_ietf_ABYTES
#define HEADER_LEN (MAGIC_LEN + NONCE_LEN)
#define PATH_MAX_LEN 512
#define AD_MAX (KEYSTORE_NAMESPACE_MAX + KEYSTORE_KEY_MAX + 2)

static struct {
    char dir[PATH_MAX_LEN / 2];
    unsigned char key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
} vault;

static int write_all(int fd, const void *data, size_t len)
{
    const uint8_t *p = data;
    while (len) {
        ssize_t n = write(fd, p, len);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return -EIO;
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

/* Writes a file so it is either fully there or not there at all. */
static int write_atomic(const char *path, const void *data, size_t len)
{
    /* Keys never start with a dot, so the temporary name can't collide with one. */
    char tmp[PATH_MAX_LEN + 8];
    const char *slash = strrchr(path, '/');
    int dir_len = slash ? (int)(slash - path) : 0;
    snprintf(tmp, sizeof(tmp), "%.*s/.%s.tmp", dir_len, path, slash ? slash + 1 : path);
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0)
        return -EIO;
    int rc = write_all(fd, data, len);
    if (rc == 0 && fsync(fd) != 0)
        rc = -EIO;
    close(fd);
    if (rc == 0 && rename(tmp, path) != 0)
        rc = -EIO;
    if (rc != 0)
        unlink(tmp);
    return rc;
}

static uint8_t *read_all(const char *path, size_t *len)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return NULL;
    struct stat st;
    uint8_t *data = NULL;
    if (fstat(fd, &st) == 0 && st.st_size >= 0 && (size_t)st.st_size <= HEADER_LEN + TAG_LEN + KEYSTORE_VALUE_MAX &&
        (data = malloc((size_t)st.st_size + 1))) {
        ssize_t n = read(fd, data, (size_t)st.st_size);
        if (n != st.st_size) {
            free(data);
            data = NULL;
        } else {
            *len = (size_t)n;
        }
    }
    close(fd);
    return data;
}

static void ns_dir(const char *ns, char *out, size_t len)
{
    snprintf(out, len, "%s/%s", vault.dir, ns);
}

static void key_path(const char *ns, const char *key, char *out, size_t len)
{
    snprintf(out, len, "%s/%s/%s", vault.dir, ns, key);
}

static size_t associated_data(const char *ns, const char *key, unsigned char *ad)
{
    size_t n = (size_t)snprintf((char *)ad, AD_MAX, "%s/%s", ns, key);
    return n < AD_MAX ? n : AD_MAX - 1;
}

bool vault_open(const char *dir)
{
    if (sodium_init() < 0)
        return false;
    snprintf(vault.dir, sizeof(vault.dir), "%s", dir);
    if (mkdir(dir, 0700) != 0 && errno != EEXIST)
        return false;
    char path[PATH_MAX_LEN];
    snprintf(path, sizeof(path), "%s/" DEVICE_KEY, dir);
    size_t len = 0;
    uint8_t *existing = read_all(path, &len);
    if (existing && len == sizeof(vault.key)) {
        memcpy(vault.key, existing, sizeof(vault.key));
        sodium_memzero(existing, len);
        free(existing);
        return true;
    }
    free(existing);
    crypto_aead_xchacha20poly1305_ietf_keygen(vault.key);
    return write_atomic(path, vault.key, sizeof(vault.key)) == 0;
}

int vault_get(const char *ns, const char *key, uint8_t **value, size_t *len)
{
    char path[PATH_MAX_LEN];
    key_path(ns, key, path, sizeof(path));
    size_t size = 0;
    uint8_t *record = read_all(path, &size);
    if (!record)
        return errno == ENOENT ? -ENOENT : -EIO;
    int rc = -EIO;
    unsigned char ad[AD_MAX];
    size_t ad_len = associated_data(ns, key, ad);
    if (size >= HEADER_LEN + TAG_LEN && memcmp(record, MAGIC, MAGIC_LEN) == 0) {
        size_t plain_len = size - HEADER_LEN - TAG_LEN;
        uint8_t *plain = malloc(plain_len + 1);
        unsigned long long out_len = 0;
        if (plain && crypto_aead_xchacha20poly1305_ietf_decrypt(plain, &out_len, NULL, record + HEADER_LEN,
                                                                size - HEADER_LEN, ad, ad_len, record + MAGIC_LEN,
                                                                vault.key) == 0) {
            *value = plain;
            *len = (size_t)out_len;
            rc = 0;
        } else {
            free(plain);
        }
    }
    free(record);
    return rc;
}

static void count_key(const char *key, void *user)
{
    (void)key;
    (*(size_t *)user)++;
}

int vault_set(const char *ns, const char *key, const uint8_t *value, size_t len)
{
    char dir[PATH_MAX_LEN], path[PATH_MAX_LEN];
    ns_dir(ns, dir, sizeof(dir));
    key_path(ns, key, path, sizeof(path));
    if (mkdir(dir, 0700) != 0 && errno != EEXIST)
        return -EIO;
    if (access(path, F_OK) != 0) {
        size_t count = 0;
        vault_list(ns, count_key, &count);
        if (count >= KEYSTORE_KEYS_MAX)
            return -ENOSPC;
    }
    size_t size = HEADER_LEN + len + TAG_LEN;
    uint8_t *record = malloc(size);
    if (!record)
        return -EIO;
    memcpy(record, MAGIC, MAGIC_LEN);
    randombytes_buf(record + MAGIC_LEN, NONCE_LEN);
    unsigned char ad[AD_MAX];
    size_t ad_len = associated_data(ns, key, ad);
    unsigned long long out_len = 0;
    crypto_aead_xchacha20poly1305_ietf_encrypt(record + HEADER_LEN, &out_len, value, len, ad, ad_len, NULL,
                                               record + MAGIC_LEN, vault.key);
    int rc = write_atomic(path, record, HEADER_LEN + (size_t)out_len);
    free(record);
    return rc;
}

int vault_delete(const char *ns, const char *key)
{
    char path[PATH_MAX_LEN];
    key_path(ns, key, path, sizeof(path));
    if (unlink(path) == 0)
        return 0;
    return errno == ENOENT ? -ENOENT : -EIO;
}

size_t vault_list(const char *ns, vault_key_cb_t cb, void *user)
{
    char dir[PATH_MAX_LEN];
    ns_dir(ns, dir, sizeof(dir));
    DIR *d = opendir(dir);
    if (!d)
        return 0;
    size_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(d))) {
        if (!keystore_valid_key(entry->d_name))
            continue;
        cb(entry->d_name, user);
        count++;
    }
    closedir(d);
    return count;
}
