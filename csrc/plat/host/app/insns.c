/* QB_INSNS set: the instructions the process retired from here to its exit, on standard error, for
   tools/bench.py. Taken in-process, the loader's and Rosetta's start-up costs are left out. */
#include <libproc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static uint64_t start;

static uint64_t retired(void)
{
    struct rusage_info_v4 ri;

    return proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri) ? 0 : ri.ri_instructions;
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

    start = retired();
    atexit(report);
}
