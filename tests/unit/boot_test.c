/* The start with too little memory. */
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include "boot.h"
#include "mem/alloc.h"
#include "session.h"
#include "ui/linemap.h"
#include "unit.h"

/* Regions no arena holds. */
static void greedy(Session *const S) { allo_new(&S->allo, UINT32_MAX / 2, nullptr); }

/* A front end whose regions do not fit the arena: boot says Out of memory and exits 1, in a child
   process of its own. Its error jumped to the REPL's landing, not yet set, and crashed: the IDE at
   4 MB, DOS/32A's exception 0Dh. */
TEST(boot_short_of_arena_says_out_of_memory)
{
    static Session child;
    static const FrontEnd fe = {.regions = greedy};

    int out[2];
    CHECK(pipe(out) == 0);
    fflush(stdout);
    const pid_t pid = fork();

    if (!pid) {
        dup2(out[1], STDERR_FILENO);
        boot(&child, &fe);
        _exit(0);
    }

    close(out[1]);
    char text[64] = {0};

    const ssize_t n = read(out[0], text, sizeof text - 1);
    close(out[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 1);
    CHECK(n > 0 && strstr(text, "Out of memory"));
}

/* Boot takes no memory a feature may never use: the POKE memory (64 KB) waits for the first SYS,
   the file records (NCH * RECMAX) for the first record. Both were carved at every start. */
TEST(boot_leaves_poke_memory_and_records_for_first_use)
{
    static Session child;
    static const FrontEnd fe = {};

    fflush(stdout);
    const pid_t pid = fork();

    if (!pid) {
        boot(&child, &fe);
        _exit(child.allo.poke || child.file.recs ? 1 : 0);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

/* What boot keeps from BASIC, in a child of its own: the heap's bytes FRE does not count. */
static uint32_t boot_keeps(void)
{
    static Session child;
    static const FrontEnd fe = {.regions = lmap_init};

    int out[2];
    CHECK(pipe(out) == 0);
    fflush(stdout);
    const pid_t pid = fork();

    if (!pid) {
        boot(&child, &fe);
        const uint32_t keeps = (64u << 20) - 0x1000 - allo_free(&child.allo);
        CHECK(write(out[1], &keeps, sizeof keeps) == sizeof keeps);
        _exit(0);
    }

    close(out[1]);
    uint32_t keeps = 0;
    CHECK(read(out[0], &keeps, sizeof keeps) == sizeof keeps);
    close(out[0]);
    waitpid(pid, nullptr, 0);
    return keeps;
}

/* Boot keeps from BASIC what it needs at first, at most the bytes below: on DOS the rest of a 2 MB
   machine is BASIC's. The code cache and the store were 64 KB each at first, the symbol tables sized
   for 1024 names and the data stack's first segment 64 KB: 587 KB kept. */
TEST(boot_keeps_little) { CHECK(boot_keeps() <= 356000); }
