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

/* An argument by reference naming the procedure's own local or by-value parameter, to a call of
   the same procedure, is the caller's variable: the call set the cell aside, and Q 3 printed
   1 2 3. QBasic 1.1's output. */
TEST(a_recursive_calls_reference_is_the_callers)
{
    static const char *const lines[] = {
        "10 q 3: p 1, 2, 2",
        "20 SUB q (n%)",
        "30 IF n% > 0 THEN k% = n% - 1: q k%: q (k%): PRINT n%;",
        "40 END SUB",
        "50 SUB p (a%, b%, d%)",
        "60 PRINT a%; b%; \"|\";",
        "70 IF d% > 0 THEN p b%, (a% * 10), d% - 1: a% = a% + 100: p a%, b%, 0",
        "80 PRINT a%; b%; \"/\";",
        "90 END SUB",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(
        S->cons.sbuf,
        S->cons.sblen,
        " 1  1  2  1  1  2  3  1  2 | 2  10 | 10  20 | 10  20 / 102  10 | 102  10 / 102  10 / "
        "101  102 | 101  102 / 101  102 /"
    );
}

#define RECORDS                                                                                    \
    "10 TYPE Moth", "11 x AS SINGLE", "12 END TYPE", "13 TYPE Pair", "14 a AS Moth",               \
        "15 b AS Moth", "16 END TYPE", "17 TYPE World", "18 p AS Pair", "19 m AS Moth",            \
        "20 END TYPE", "21 DIM w AS World, a(2) AS Moth, g(1, 2) AS Moth, r(1) AS World"
#define MOVE "900 SUB Move (m AS Moth)", "910 m.x = m.x + 5", "920 END SUB"

/* A record's field, a record array's element or its field passed to a record parameter was a Syntax
   error: each by reference, through a SUB, a FUNCTION, and a parameter passed on. */
TEST(record_field_and_element_by_reference)
{
    static const char *const lines[] = {
        RECORDS,
        "30 Move w.m: CALL Move(w.p.b): Move a(1): Move g(1, 2): Move r(1).m: Move r(0).p.a",
        "40 PRINT w.m.x; w.p.b.x; a(1).x; g(1, 2).x; r(1).m.x; r(0).p.a.x; Bump(w.m); w.m.x",
        "50 Twice a(2): Deep r(1): PRINT a(2).x; r(1).m.x",
        MOVE,
        "930 FUNCTION Bump (m AS Moth): m.x = m.x + 1: Bump = m.x: END FUNCTION",
        "940 SUB Twice (m AS Moth): Move m: Move m: END SUB",
        "950 SUB Deep (v AS World): Move v.m: END SUB",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 5  5  5  5  5  5  6  6 \r 10  10 \r");
}

/* A record of another TYPE passed to a record parameter: Parameter type mismatch; a whole record's
   was taken silently. */
TEST(record_argument_of_another_type)
{
    static const char *const field[] = {RECORDS, "30 Move w.p", MOVE, nullptr};
    static const char *const elem[] = {RECORDS, "30 Move r(1)", MOVE, nullptr};
    static const char *const whole[] = {RECORDS, "30 Move w", MOVE, nullptr};
    static const char *const number[] = {RECORDS, "30 Move w.m.x", MOVE, nullptr};

    CHECK(unit_run(S, field) == E_PTM);
    CHECK(unit_run(S, elem) == E_PTM);
    CHECK(unit_run(S, whole) == E_PTM);
    CHECK(unit_run(S, number) == E_PTM);
}
