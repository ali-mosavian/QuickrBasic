/* A small test harness: TEST(name) { ... CHECK(...); } in any tests/unit file; run.c runs them
   all after boot(). */
#ifndef UNIT_H
#define UNIT_H
#include <stdint.h>
#include <string.h>
#include "base.h"
#include "fault.h"

typedef void TestFn(Session *S);

typedef struct UnitTest {
    const char *name;
    TestFn *fn;
    struct UnitTest *next;
} UnitTest;

/* Tests in the order added: end is the last's next, or first. */
typedef struct {
    UnitTest *first, **end;
} UnitList;

/* Every TEST's. */
extern UnitList unit_tests;
/* t the last of l's. */
void unit_add(UnitList *l, UnitTest *t);
void unit_fail(const char *file, int line, const char *what);

/* The program's lines (nullptr ended) as the only ones. */
void unit_store(Session *S, const char *const *lines);
/* unit_store, then RUN: its output in S->cons.sbuf. */
Status unit_run(Session *S, const char *const *lines);
/* unit_store, then RUN: its output where it goes, the screen when standard output is. */
Status unit_run_shown(Session *S, const char *const *lines);
/* A direct line typed and run: its output in S->cons.sbuf. */
Status unit_typed(Session *S, const char *line);
/* lines, unnumbered as a file's may be, stored as a file's load does, then RUN: its output in
   S->cons.sbuf. */
Status unit_run_file(Session *S, const char *const *lines);
/* The ops op in the stored program's code, as linked. */
uint32_t unit_ops(const Session *S, uint8_t op);
/* The program's CALL k ops of built-in k, as unit_ops counts. */
uint32_t unit_calls(const Session *S, uint8_t k);
/* The n bytes of src written to a file, loaded as a program file is (repl_load), then RUN: its
   output in S->cons.sbuf. */
Status unit_run_bytes(Session *S, const char *src, size_t n);

#define TEST(name)                                                                                 \
    static void name(Session *S);                                                                  \
    static UnitTest name##_test = {#name, name, nullptr};                                          \
    [[gnu::constructor]]                                                                           \
    static void name##_register(void)                                                              \
    {                                                                                              \
        unit_add(&unit_tests, &name##_test);                                                       \
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
