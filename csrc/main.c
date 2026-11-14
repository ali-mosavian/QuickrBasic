/* The start: everything set up, then the READY loop. */
#include "boot.h"
#include "io/repl.h"
#include "session.h"

static Session session; /* the program's one session */

int main(void)
{
    Session *const S = &session;
    boot(S);
    repl_main(S);
}
