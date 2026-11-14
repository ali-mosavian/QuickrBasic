/* Expressions' function and element nodes: what they evaluate to. */
#include "session.h"
#include "unit.h"

/* expr PRINTed: want, then PRINT's CR. */
static void eval_is(Session *const S, const char *const expr, const char *const want)
{
    char out[80];
    CHECK(session_eval(S, expr, 0, out) == 0);
    const size_t n = strlen(want);
    CHECK(!memcmp(out, want, n) && out[n] == '\r' && !out[n + 1]);
}

/* The golden corpus has no MID$ without its length: K_MID's call in its place read the string
   under it as the length and passed alone. */
TEST(string_functions_take_their_optional_arguments)
{
    eval_is(S, "\"<\" + MID$(\"abcdef\", 3)", "<cdef");
    eval_is(S, "MID$(\"abcdef\", 3, 2)", "cd");
    eval_is(S, "LEFT$(\"abc\" + \"def\", 4)", "abcd");
    eval_is(S, "INSTR(\"abcabc\", \"c\")", " 3 ");
    eval_is(S, "INSTR(4, \"abcabc\", \"c\")", " 6 ");
}

TEST(numeric_functions_of_folded_arguments)
{
    eval_is(S, "SGN(2 - 5) + ABS(-3)", " 2 ");
    eval_is(S, "STR$(2 * 3)", " 6");
}
