/* basic32: the session with the console's READY loop; a file the command line names is run.
 */
#include "boot.h"
#include "session.h"

static Session session; /* the program's one session */

int main(void)
{
    Session *const S = &session;
    boot(S, &repl_batch);
    repl_main(S);
}
