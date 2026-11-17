/* QB_INSNS set: the instructions this thread retired from here to the exit, on standard error, for
   tools/bench.py; "insns unavailable" when the machine will not count (a CI VM), never a count of 0.
   The thread's own (thread_selfcounts, perf_event_open); in-process, the loader's start-up left out. */
#ifdef __linux__
#define _GNU_SOURCE
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <unistd.h>
#else
#include <dlfcn.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
static int fd = -1;

static bool counter_open(void)
{
    struct perf_event_attr a = {
        .type = PERF_TYPE_HARDWARE,
        .size = sizeof a,
        .config = PERF_COUNT_HW_INSTRUCTIONS,
        .exclude_kernel = 1,
        .exclude_hv = 1,
    };
    fd = (int)syscall(SYS_perf_event_open, &a, 0, -1, -1, 0);
    return fd >= 0;
}

static bool counter_read(uint64_t *const n) { return read(fd, n, sizeof *n) == sizeof *n; }
#else
enum { THSC_CPI = 1 }; /* instructions, then cycles */

static int (*selfcounts)(int, void *, size_t);

static bool counter_open(void)
{
    selfcounts = (int (*)(int, void *, size_t))dlsym(RTLD_DEFAULT, "thread_selfcounts");
    return selfcounts;
}

static bool counter_read(uint64_t *const n)
{
    uint64_t c[2] = {};

    if (selfcounts(THSC_CPI, c, sizeof c))
        return false;

    *n = c[0];
    return true;
}
#endif

static uint64_t start;

static void unavailable(void) { fprintf(stderr, "\ninsns unavailable\n"); }

static void report(void)
{
    uint64_t end;

    if (!counter_read(&end))
        unavailable();
    else
        fprintf(stderr, "\ninsns %llu\n", (unsigned long long)(end - start));
}

[[gnu::constructor]]
static void count_from_here(void)
{
    if (!getenv("QB_INSNS"))
        return;

    atexit(counter_open() && counter_read(&start) ? report : unavailable);
}
