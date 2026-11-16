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

/* Inline MOD, SINGLE division and RETURN fail at their own statements: RESUME NEXT goes on after
   each. */
TEST(inline_errors_resume_next)
{
    static const char *const lines[] = {
        "10 ON ERROR GOTO 100",
        "20 a% = 0: b% = 7 MOD a%: PRINT \"m\";",
        "30 x! = 0: y! = 1 / x!: PRINT \"d\";",
        "40 RETURN: PRINT \"r\";",
        "50 PRINT \"end\";: END",
        "100 PRINT ERR;: RESUME NEXT",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 11 m 11 d 3 rend");
}

/* GOSUB and RETURN inline; a false IF on the program's last line ends it. */
TEST(inline_gosub_and_last_line_if)
{
    static const char *const lines[] = {
        "10 GOTO 30",
        "20 PRINT i% MOD 2;: RETURN",
        "30 FOR i% = 1 TO 3: GOSUB 20: NEXT",
        "40 x! = 2.5: IF x! > 3 THEN PRINT \"no\"",
        "50 IF i% < 2 THEN PRINT \"no\"",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 1  0  1 ");
}
