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

/* An INT operator with a 16-bit literal, MOD with a byte's: the literal unsigned, the result as
   the unfused pair's; MOD by a literal 0 fails at its statement, RESUME NEXT after it. */
TEST(fused_literal_ops)
{
    static const char *const lines[] = {
        "10 ON ERROR GOTO 100",
        "20 a& = -7: PRINT a& + 40000; a& - 40000; a& * 1000; 70000 \\ 1000; a& MOD 1000;",
        "30 PRINT a& MOD 3; 300 AND 260; 256 OR 1; (a& > 1000); (40000 = a& + 40007);",
        "40 b% = 5 MOD 0: PRINT \"no\";",
        "50 PRINT \"end\";: END",
        "100 PRINT ERR;: RESUME NEXT",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(
        S->cons.sbuf,
        S->cons.sblen,
        " 39993 -40007 -7000  70 -7 -1  260  257  0 -1  11 noend"
    );
}

/* A compare and its JF or JT as one op: IF's, WHILE's, a FOR's entry (JT, the mask's
   complement), by a variable, a byte, a 16-bit literal and the stack. */
TEST(fused_compare_branches)
{
    static const char *const lines[] = {
        "10 a% = 3: b% = 3",
        "20 IF a% = b% THEN PRINT \"v\";",
        "30 IF a% < 2 THEN PRINT \"no\"; ELSE PRINT \"u8\";",
        "40 IF a% + 1000 >= 1003 THEN PRINT \"i16\";",
        "50 IF a% * 2 <> b% * 2 THEN PRINT \"no\"; ELSE PRINT \"pop\";",
        "60 WHILE a% < 6: a% = a% + 1: WEND: PRINT a%;",
        "70 FOR i% = 3 TO 1: PRINT \"no\";: NEXT: FOR i% = 2 TO 1 STEP -1: PRINT i%;: NEXT",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, "vu8i16pop 6  2  1 ");
}

/* x = a op b, x = k and x = a as one op each: an overflow or a division by zero stores nothing, and
   RESUME NEXT goes on after the statement; a SINGLE's bits copy as they are. */
TEST(fused_stores)
{
    static const char *const lines[] = {
        "10 ON ERROR GOTO 100",
        "20 a& = 2147483647: b& = 1: c& = 5",
        "30 c& = a& + b&: PRINT \"no\";",
        "40 c& = a& \\ 0: PRINT \"no\";",
        "50 PRINT c&;: c& = a& - 47: d& = c&: e& = 300 * 2 + b& AND 255",
        "60 x! = 2.5: y! = x!: z! = 123456.7: PRINT d&; e&; y!; z!;: END",
        "100 PRINT ERR;: RESUME NEXT",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 6 no 11 no 5  2147483600  89  2.5  123456.7 ");
}
