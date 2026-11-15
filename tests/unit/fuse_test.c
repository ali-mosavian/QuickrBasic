/* The fused ops (comp/fuse.c) as the sequences they replace. */
#include "session.h"
#include "unit.h"

/* I.NEXT written over its own sequence copied the op again after it: each NEXT stepped thrice. */
TEST(fused_next_steps_once)
{
    static const char *const lines[] = {
        "10 FOR i% = 1 TO 3: PRINT i%;: NEXT: PRINT i%;",
        "20 FOR j% = 10 TO 1 STEP -4: PRINT j%;: NEXT j%: PRINT j%;",
        "30 FOR k% = 5 TO 1: PRINT \"no\";: NEXT: PRINT k%;",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 1  2  3  4  10  6  2 -2  5 ");
}

/* A fused NEXT stepped past an INT's range is Illegal function call, the counter kept. */
TEST(fused_next_past_range)
{
    static const char *const lines[] = {
        "10 ON ERROR GOTO 100",
        "20 FOR i& = 2147483646 TO 2147483647: NEXT",
        "30 PRINT \"no\": END",
        "100 PRINT ERR; i&;: END",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 5  2147483647 ");
}
