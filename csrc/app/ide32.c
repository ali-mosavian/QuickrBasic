/* ide32: the session with the editor as its front end.
 */
#include "boot.h"
#include "session.h"
#include "ui/uimain.h"

static Session session; /* the program's one session */

int main(void)
{
    Session *const S = &session;
    boot(S, &uimn_front_end);
    repl_main(S);
}
