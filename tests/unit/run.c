/* Runs every TEST, once the core is booted; exits 1 when one failed. */
#include <stdio.h>
#include "boot.h"
#include "session.h"
#include "unit.h"

enum { MAX_TESTS = 256 };

static struct {
    const char *name;
    TestFn *fn;
} tests[MAX_TESTS];
static uint32_t ntests, failures;
static const char *current;

void unit_register(const char *const name, TestFn *const fn)
{
    if (ntests < MAX_TESTS)
        tests[ntests++] = (typeof(tests[0])){name, fn};
}

void unit_fail(const char *const file, const int line, const char *const what)
{
    failures++;
    fprintf(stderr, "FAIL %s: %s:%d: %s\n", current, file, line, what);
}

/* text entered as a typed line. */
static uint8_t entered(Session *const S, const char *const text)
{
    const uint32_t n = (uint32_t)strlen(text);
    memcpy(S->cons.inbuf, text, n);
    return repl_enter(S, n);
}

static void direct(Session *const S) { repl_run(S); }

/* A typed direct line run. */
static Status typed(Session *const S, const char *const text)
{
    CHECK(entered(S, text) == ENTER_DIRECT);
    return repl_call(S, direct);
}

Status unit_run(Session *const S, const char *const *const lines)
{
    session_new(S);

    for (size_t i = 0; lines[i]; i++)
        CHECK(entered(S, lines[i]) == ENTER_STORED);

    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    const Status st = typed(S, "RUN");
    S->cons.capture = 0;
    return st;
}

static Session session;

int main(void)
{
    Session *const S = &session;
    static const FrontEnd none = {};

    boot(S, &none);

    for (uint32_t i = 0; i < ntests; i++) {
        current = tests[i].name;
        tests[i].fn(S);
    }

    printf("%u tests, %u failures\n", ntests, failures);
    return failures ? 1 : 0;
}
