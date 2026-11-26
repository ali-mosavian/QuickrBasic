/* The start with too little memory. */
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include "boot.h"
#include "mem/alloc.h"
#include "session.h"
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
