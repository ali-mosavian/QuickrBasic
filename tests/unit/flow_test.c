/* Jumps and calls taken by the VM. */
#include "session.h"
#include "unit.h"

/* An event is run at the stop check of a jump in a one-line loop: its handler moves the clock, so
   the next tick runs it again. */
#define TICKING(loop)                                                                              \
    {                                                                                              \
        "10 ON TIMER(1) GOSUB 100: TIMER ON: TIME$ = \"02:00:00\"",                                \
        "20 " loop,                                                                                \
        "30 PRINT n;: END",                                                                        \
        "100 n = n + 1: IF n MOD 2 THEN TIME$ = \"01:00:00\" ELSE TIME$ = \"02:00:00\"",           \
        "110 RETURN",                                                                              \
        "200 SUB s (x)",                                                                           \
        "210 END SUB",                                                                             \
        nullptr,                                                                                   \
    }

/* A loop's conditional jump popped its flag before the stop check: the event ran the jump again,
   which popped one cell more each time. */
TEST(event_at_a_conditional_jump_keeps_the_stack)
{
    static const char *const lines[] = TICKING("DO: LOOP UNTIL n >= 3");

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 3 ");
    CHECK(S->vm.sp == S->vm.stktop);
}

/* A SUB call took its arguments and pushed its frame before the stop check: the event ran the call
   again on a stack without them. */
TEST(event_at_a_call_keeps_the_stack)
{
    static const char *const lines[] = TICKING("DO: s 1: LOOP UNTIL n >= 6");

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 6 ");
    CHECK(S->vm.sp == S->vm.stktop);
}
