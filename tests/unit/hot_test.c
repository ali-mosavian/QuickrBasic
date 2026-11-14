/* The VM's inline ops (vm/hot.h) as the ops table's. */
#include "session.h"
#include "unit.h"

/* An INT op failing inline is raised at its own statement: RESUME NEXT goes on after it. */
TEST(inline_error_at_its_statement)
{
    static const char *const lines[] = {
        "10 ON ERROR GOTO 100",
        "20 a& = 2147483647",
        "30 a& = a& + 1: PRINT \"next\";",
        "40 PRINT \"end\";: END",
        "100 PRINT ERR;: RESUME NEXT",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 6 nextend");
}
