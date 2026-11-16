/* QB_INSNS set: the instructions this thread retired from here to the exit, on standard error, for
   tools/bench.py. The thread's own (thread_selfcounts); in-process, the loader's
   start-up left out. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

enum { THSC_CPI = 1 }; /* instructions, then cycles */

static int (*selfcounts)(int, void *, size_t);
static uint64_t start;

static uint64_t retired(void)
{
    uint64_t c[2] = {};

    return selfcounts(THSC_CPI, c, sizeof c) ? 0 : c[0];
}

static void report(void)
{
    fprintf(stderr, "\ninsns %llu\n", (unsigned long long)(retired() - start));
}

[[gnu::constructor]]
static void count_from_here(void)
{
    if (!getenv("QB_INSNS"))
        return;

    selfcounts = (int (*)(int, void *, size_t))dlsym(RTLD_DEFAULT, "thread_selfcounts");

    if (!selfcounts) {
        fprintf(stderr, "insns: no thread_selfcounts\n");
        exit(1);
    }

    start = retired();
    atexit(report);
}
