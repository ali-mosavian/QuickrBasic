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

/* Each activation has its own local arrays: the inner calls' DIMs changed none of the outer's. */
TEST(each_call_its_own_local_arrays)
{
    static const char *const lines[] = {
        "10 PRINT f%(4)",
        "20 FUNCTION f% (n%)",
        "30 DIM m%(2): m%(1) = n%",
        "40 IF n% > 1 THEN f% = f%(n% - 1) + m%(1) ELSE f% = m%(1)",
        "50 END FUNCTION",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 10 \r");
}

/* A return frees every local array: x's header, after y's once REDIM made it again, moved down
   when y's was freed and was fixed in the frame, still there; else x freed a stale header and the
   memory and the module's array were wrong. */
TEST(a_returns_local_arrays_are_all_freed)
{
    static const char *const lines[] = {
        "10 a = FRE(-1): s: s: PRINT a - FRE(-1); g(1)",
        "20 SUB s",
        "30 REDIM x(100): REDIM y(200): REDIM x(50): y(5) = 1",
        "40 END SUB",
        "5 DIM g(3): g(1) = 9",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 0  9 \r");
}

/* An array parameter is its argument's cell: REDIM and ERASE in the callee are the caller's. */
TEST(an_array_parameter_is_its_argument)
{
    static const char *const lines[] = {
        "10 REDIM d(3): d(1) = 11: g d(): PRINT UBOUND(d); d(1); d(9): e d(): PRINT FRE(-1) > 0",
        "20 SUB g (z())",
        "30 REDIM PRESERVE z(10): z(9) = 99",
        "40 END SUB",
        "50 SUB e (z())",
        "60 ERASE z: PRINT LBOUND(z)",
        "70 END SUB",
        nullptr,
    };

    CHECK(unit_run(S, lines) != ST_OK); /* LBOUND of the ERASEd: Subscript out of range */
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 10  11  99 \r");
}

/* A local array DIMmed with constant bounds is in the frame: the heap gives it nothing. The bounds'
   literals were missed (wrapped by the INT conversion), and every such array took the heap. */
TEST(a_constant_local_array_is_in_the_frame)
{
    static const char *const lines[] = {
        "10 a = FRE(-1): s a",
        "20 SUB s (a)",
        "30 DIM x(100), y(-2 TO 2): x(100) = 1: y(-2) = 2: PRINT a - FRE(-1); x(100); y(-2)",
        "40 END SUB",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 0  1  2 \r");
}

/* Freeing a heap array moves the headers after it; a frame's header, past their region, stays. */
TEST(a_frame_arrays_header_stays_when_the_heap_moves)
{
    static const char *const lines[] = {
        "10 s",
        "20 SUB s",
        "30 REDIM x(3): DIM y(5): y(2) = 7: ERASE x: PRINT y(2)",
        "40 END SUB",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 7 \r");
}

/* The last heap header follows a freed one: a test of the region's new end missed it. */
TEST(the_last_heap_header_follows_a_free)
{
    static const char *const lines[] = {
        "10 REDIM a(3): REDIM b(3): b(1) = 5: ERASE a: PRINT b(1)",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 5 \r");
}

/* A frame array REDIMmed through a parameter: Duplicate definition, not arry_free on the frame. */
TEST(a_frame_array_is_not_redimmed)
{
    static const char *const lines[] = {
        "10 s",
        "20 SUB s",
        "30 DIM y(5): t y()",
        "40 END SUB",
        "50 SUB t (z())",
        "60 REDIM z(9)",
        "70 END SUB",
        nullptr,
    };

    CHECK(unit_run(S, lines) == E_RD);
}
