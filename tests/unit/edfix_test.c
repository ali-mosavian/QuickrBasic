/* The editor's fixture itself. */
#include "edfix.h"
#include "unit.h"

/* fx_done left the editor's debugger in the session: its hooks, and a DBG op compiled before
   each statement. A test after it that left a step pending then stopped at every statement,
   in the editor's hooks: debug_test.c's frames came out wrong, then a crash. */
TEST(edfix_done_takes_the_debugger_out)
{
    fx_open_program(S, "sub hi\r\nend sub\r\n");
    CHECK(S->dbug.emit && S->dbug.hooks);
    fx_done(S, (const char *[]){"main.bas", "gfx.bas", nullptr});

    CHECK(!S->dbug.emit && !S->dbug.hooks);
}
