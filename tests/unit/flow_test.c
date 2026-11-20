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
    CHECK(S->vm.sp == S->vm.rsp);
}

/* A SUB call took its arguments and pushed its frame before the stop check: the event ran the call
   again on a stack without them. */
TEST(event_at_a_call_keeps_the_stack)
{
    static const char *const lines[] = TICKING("DO: s 1: LOOP UNTIL n >= 6");

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 6 ");
    CHECK(S->vm.sp == S->vm.rsp);
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

/* A line's procedure was found by its offset cut to 16 bits: past 64 KB of program, a SUB's lines
   covered the module's PRINT x, which read the SUB's x, 0. */
TEST(procedures_past_64k_keep_their_lines)
{
    static char src[84 * 1024];
    size_t n = 0;
#define PUT(s) (memcpy(src + n, (s), sizeof(s) - 1), n += sizeof(s) - 1)
#define PAD "' ..............................................................................\r\n"
    PUT("x = 7\r\n");

    while (n < 10 * 1024)
        PUT(PAD);

    PUT("PRINT x\r\n");

    while (n < 56 * 1024)
        PUT(PAD);

    PUT("SUB s\r\n");

    while (n < 80 * 1024)
        PUT(PAD);

    PUT("END SUB\r\n");
#undef PAD
#undef PUT
    CHECK(unit_run_bytes(S, src, n) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 7 \r");
}

/* DIM's bounds and the subscripts were cut to 16 bits: DIM a(70000) gave UBOUND 4464, and a(69000)
   was Subscript out of range. */
TEST(bounds_past_16_bits)
{
    static const char *const lines[] = {
        "10 DIM a(70000): a(70000) = 5: a(69000) = 6: PRINT UBOUND(a); a(70000); a(69000)",
        "20 DIM b(-40000 TO -39999, 100000 TO 100001): b(-40000, 100001) = 7",
        "30 PRINT LBOUND(b, 1); UBOUND(b, 2); b(-40000, 100001)",
        "40 REDIM c(1 TO 2): REDIM c(65537 TO 65539): c(65539) = 8: PRINT LBOUND(c); c(65539)",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 70000  5  6 \r-40000  100001  7 \r 65537  8 \r");
}

/* Arrays were row-major: a(1, 0) was 12 bytes past a(0, 0) and a(0, 1) 4. QB 4.5's first subscript
   varies fastest, on the heap, in a frame and through REDIM PRESERVE. */
TEST(arrays_are_column_major)
{
    static const char *const lines[] = {
        "10 DIM a(2, 2): p = VARPTR(a(0, 0)): PRINT VARPTR(a(1, 0)) - p; VARPTR(a(0, 1)) - p",
        "20 s",
        "30 REDIM b(1, 1): b(1, 0) = 5: b(0, 1) = 6: REDIM PRESERVE b(1, 2)",
        "40 PRINT b(1, 0); b(0, 1)",
        "100 SUB s",
        "110 DIM c(2, 2): p = VARPTR(c(0, 0)): PRINT VARPTR(c(1, 0)) - p; VARPTR(c(0, 1)) - p",
        "120 END SUB",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 4  12 \r 4  12 \r 5  6 \r");
}
