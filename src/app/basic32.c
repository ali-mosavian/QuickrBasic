/* basic32: the session with the console's READY loop; a file the command line names is run.
 */
#include "boot.h"
#include "session.h"

static Session session; /* the program's one session */

#ifdef EXEC_COUNT
#include <stdio.h>
#include <stdlib.h>

/* The ops run, for tools/dosrun's instruction count. */
static void count_report(void)
{
    fprintf(stderr, "\nops %llu\n", (unsigned long long)session.vm.count);
}

static const char *const op_names[] = {
#define OP(name, h, size, flags) #name,
#include "vm/ops.def"
};

static const char *const x_names[] = {
#define XOP(name, h, size, flags) #name,
#include "vm/ops.def"
};

/* Each op's runs, for tools/bench.py ops: "op NAME n", an X op's "xop NAME n". */
static void each_report(void)
{
    for (uint32_t i = 0; i < O_NOPS; i++)
        fprintf(stderr, "op %s %llu\n", op_names[i], (unsigned long long)session.vm.each[i]);

    for (uint32_t i = 0; i < X_NOPS; i++)
        fprintf(stderr, "xop %s %llu\n", x_names[i], (unsigned long long)session.vm.xeach[i]);
}
#endif

int main(void)
{
    Session *const S = &session;
#ifdef EXEC_COUNT

    if (getenv("QB_OPS"))
        atexit(count_report);
    if (getenv("QB_EACH_OP"))
        atexit(each_report);
#endif

    boot(S, &repl_batch);
    repl_main(S);
}
