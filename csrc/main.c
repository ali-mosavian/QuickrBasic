/* The start: everything set up, then the READY loop. */
#include "boot.h"
#include "io/repl.h"
#include "session.h"
#if IDE
#include "ui/uimn.h"
#endif

static Session session; /* the program's one session */

int main(void)
{
#if IDE
    const FrontEnd *const fe = &uimn_front_end;
#else
    const FrontEnd *const fe = &repl_batch;
#endif
    Session *const S = &session;
    boot(S, fe);
    repl_main(S);
}
