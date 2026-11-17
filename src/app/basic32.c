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
#endif

int main(void)
{
    Session *const S = &session;
#ifdef EXEC_COUNT

    if (getenv("QB_OPS"))
        atexit(count_report);
#endif

    boot(S, &repl_batch);
    repl_main(S);
}
