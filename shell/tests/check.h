#ifndef SLATE_TEST_CHECK_H
#define SLATE_TEST_CHECK_H

#include <stdio.h>

static int failures;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
            failures++;                                                             \
        }                                                                           \
    } while (0)

#endif
