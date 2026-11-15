/* A small test harness: TEST(name) { ... CHECK(...); } in any tests/unit file; run.c runs them
   all after boot(). */
#ifndef UNIT_H
#define UNIT_H
#include <stdint.h>
#include <string.h>
#include "base.h"
#include "fault.h"

typedef void TestFn(Session *S);
void unit_register(const char *name, TestFn *fn);
void unit_fail(const char *file, int line, const char *what);

/* The program's lines (nullptr ended) as the only ones. */
void unit_store(Session *S, const char *const *lines);
/* unit_store, then RUN: its output in S->cons.sbuf. */
Status unit_run(Session *S, const char *const *lines);
/* unit_store, then RUN: its output where it goes, the screen when standard output is. */
Status unit_run_shown(Session *S, const char *const *lines);
/* lines, unnumbered as a file's may be, stored as a file's load does, then RUN: its output in
   S->cons.sbuf. */
Status unit_run_file(Session *S, const char *const *lines);

#define TEST(name)                                                                                 \
    static void name(Session *S);                                                                  \
    [[gnu::constructor]]                                                                           \
    static void name##_register(void)                                                              \
    {                                                                                              \
        unit_register(#name, name);                                                                \
    }                                                                                              \
    static void name([[maybe_unused]] Session *const S)

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond))                                                                               \
            unit_fail(__FILE__, __LINE__, #cond);                                                  \
    } while (0)

/* Text a function wrote, n bytes, against want. */
#define CHECK_TEXT(got, n, want) CHECK((n) == strlen(want) && !memcmp((got), (want), (n)))
#endif
