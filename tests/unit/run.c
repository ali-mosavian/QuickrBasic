/* Runs every TEST, once the core is booted; exits 1 when one failed. */
#include <stdio.h>
#include "boot.h"
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

int main(void)
{
    boot();
    for (uint32_t i = 0; i < ntests; i++) {
        current = tests[i].name;
        tests[i].fn();
    }
    printf("%u tests, %u failures\n", ntests, failures);
    return failures ? 1 : 0;
}
