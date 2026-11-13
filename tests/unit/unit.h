/* A small test harness: TEST(name) { ... CHECK(...); } in any tests/unit file; run.c runs them
   all after boot(). */
#ifndef UNIT_H
#define UNIT_H
#include <stdint.h>
#include <string.h>

typedef void TestFn(void);
void unit_register(const char *name, TestFn *fn);
void unit_fail(const char *file, int line, const char *what);

#define TEST(name)                                                                                 \
    static void name(void);                                                                        \
    [[gnu::constructor]] static void name##_register(void) { unit_register(#name, name); }        \
    static void name(void)

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond))                                                                               \
            unit_fail(__FILE__, __LINE__, #cond);                                                  \
    } while (0)

/* Text a function wrote, n bytes, against want. */
#define CHECK_TEXT(got, n, want) CHECK((n) == strlen(want) && !memcmp((got), (want), (n)))
#endif
