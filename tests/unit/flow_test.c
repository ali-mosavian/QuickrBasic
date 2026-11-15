/* Jumps, calls and loops taken by the VM. */
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

/* A step whose sign is known only when run counts each way; a loop past its limit is not entered. */
TEST(for_by_a_step_known_when_run)
{
    static const char *const lines[] = {
        "10 FOR d = -1 TO 1 STEP 2",
        "20 FOR i = -2 * d TO 2 * d STEP d: PRINT i;: NEXT i",
        "30 FOR i = 2 * d TO -2 * d STEP d: PRINT \"x\";: NEXT i",
        "40 NEXT d",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 2  1  0 -1 -2 -2 -1  0  1  2 ");
}

/* A DOUBLE's FOR was a Type mismatch (GORILLAS.BAS's explosion): its counter counts in DOUBLEs, by
   a step known when compiled or only when run, past where a SINGLE's would stall. */
TEST(for_by_a_double)
{
    static const char *const lines[] = {
        "10 FOR c# = 1 TO 2 STEP .5: PRINT c#;: NEXT",
        "20 d# = -.5: FOR c# = 1 TO 0 STEP d#: PRINT c#;: NEXT",
        "30 FOR c# = 16777216# TO 16777218#: PRINT c# - 16777216#;: NEXT c#",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 1  1.5  2  1  .5  0  0  1  2 ");
}

/* An INT's NEXT stepped past its range is Illegal function call, not Overflow. */
TEST(next_past_an_ints_range)
{
    static const char *const lines[] = {
        "10 FOR k& = 2147483646 TO 2147483647: NEXT",
        nullptr,
    };

    CHECK(unit_run(S, lines) == E_IQ);
}
