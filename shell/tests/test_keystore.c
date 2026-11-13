#include <string.h>

#include "check.h"
#include "keystore.h"

static void test_keys(void)
{
    CHECK(keystore_valid_key("token"));
    CHECK(keystore_valid_key("ava.refresh_token-2"));
    CHECK(!keystore_valid_key(""));
    CHECK(!keystore_valid_key(NULL));
    CHECK(!keystore_valid_key(".hidden"));
    CHECK(!keystore_valid_key("../shell/token"));
    CHECK(!keystore_valid_key("a/b"));
    CHECK(!keystore_valid_key("with space"));
    char longest[KEYSTORE_KEY_MAX + 2];
    memset(longest, 'k', KEYSTORE_KEY_MAX);
    longest[KEYSTORE_KEY_MAX] = '\0';
    CHECK(keystore_valid_key(longest));
    longest[KEYSTORE_KEY_MAX] = 'k';
    longest[KEYSTORE_KEY_MAX + 1] = '\0';
    CHECK(!keystore_valid_key(longest));
}

static void test_namespaces(void)
{
    char ns[KEYSTORE_NAMESPACE_MAX];
    CHECK(keystore_namespace("slate", ns, sizeof(ns)) && strcmp(ns, "shell") == 0);
    CHECK(keystore_namespace("app-ava", ns, sizeof(ns)) && strcmp(ns, "app-ava") == 0);
    CHECK(keystore_namespace("app-shell", ns, sizeof(ns)) && strcmp(ns, "app-shell") == 0);
    CHECK(!keystore_namespace("root", ns, sizeof(ns)));
    CHECK(!keystore_namespace("app-../x", ns, sizeof(ns)));
    CHECK(!keystore_namespace(NULL, ns, sizeof(ns)));
}

int main(void)
{
    test_keys();
    test_namespaces();
    return failures ? 1 : 0;
}
