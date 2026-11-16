/* SUB and FUNCTION frames (vm/flow.c): locals and arguments on the data stack. */
#include "session.h"
#include "unit.h"

/* Each activation has its own locals, a string's too: the inner calls changed none of the
   outer's. */
TEST(each_call_its_own_locals)
{
    static const char *const lines[] = {
        "10 PRINT f%(4)",
        "20 FUNCTION f% (n%)",
        "30 t$ = STR$(n%): k% = n%",
        "40 IF n% > 1 THEN r% = n% * f%(n% - 1) ELSE r% = 1",
        "50 PRINT k%; t$;: f% = r%",
        "60 END FUNCTION",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 1  1 2  2 3  3 4  4 24 \r");
}

/* A returned call's string local owns no block: its Desc, below the stack's top, still pointed at
   it, and the collection kept the 1000 bytes. */
TEST(a_returned_calls_strings_are_freed)
{
    static const char *const lines[] = {
        "10 a = FRE(\"\"): s: b = FRE(\"\"): PRINT a - b",
        "20 SUB s",
        "30 u% = 1: t$ = STRING$(1000, 65)", /* t$ the second cell: FRE's argument misses it */
        "40 END SUB",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 0 \r");
}

/* An element passed by reference follows its array when an ERASE moves the arrays: the frame's
   address of it was left behind, and the 8 went elsewhere. */
TEST(an_elements_reference_follows_its_array)
{
    static const char *const lines[] = {
        "10 REDIM SHARED b(100): REDIM a(5): a(2) = 7",
        "20 e a(2): PRINT a(2)",
        "30 SUB e (x)",
        "40 ERASE b: x = x + 1",
        "50 END SUB",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 8 \r");
}
