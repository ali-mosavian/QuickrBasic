/* Expressions' function and element nodes: what they evaluate to. */
#include "session.h"
#include "comp/lexer.h"
#include "vm/flow.h"
#include "unit.h"

/* expr PRINTed: want, then PRINT's CR. */
static void eval_is(Session *const S, const char *const expr, const char *const want)
{
    char out[80];
    CHECK(session_eval(S, expr, 0, out) == ST_OK);
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

/* A keyword not a function where an operand goes: its Syntax error was marked past it, at the
   statement's end, so x = MKDIR read as a call wanting its '('. */
TEST(not_a_function_marked_at_itself)
{
    uint8_t text[] = "x = MKDIR";
    lexr_line(&S->lexr, text);
    S->comp.check = true;
    (void)comp_line(&S->comp);
    S->comp.check = false;
    const uint8_t *op = S->comp.xbuf;

    while (!exec_is_err(op) && *op != O_EOL)
        op += flow_opsize(op);

    CHECK(exec_is_err(op));
    CHECK(((const ErrOp *)op)->at == 8); /* x: 7 bytes, =, then MKDIR */
}

/* A variable of another type than its parameter: the error at the argument; it was at the ','
   after it. */
TEST(parameter_mismatch_marked_at_its_argument)
{
    static const char *const lines[] = {
        "10 a = 1: b = 2",
        "20 FUNCTION sum% (a AS INTEGER, b AS INTEGER)",
        "30 sum% = a + b",
        "40 END FUNCTION",
        nullptr,
    };

    (void)unit_run(S, lines); /* linked: the call checked against its parameters */
    uint8_t text[] = "x = sum%(a, b)";
    lexr_line(&S->lexr, text);
    S->comp.check = true;
    (void)comp_line(&S->comp);
    S->comp.check = false;
    const uint8_t *op = S->comp.xbuf;

    while (!exec_is_err(op) && *op != O_EOL)
        op += flow_opsize(op);

    CHECK(exec_is_err(op));
    CHECK(((const ErrOp *)op)->at == 19); /* x: 7 bytes, =, sum%: 10, (, then a */
}

/* A FUNCTION's result set in its body passes the IDE's check of the line alone, after a run linked
   the procedures: the check, at the module, took it for an assignment from outside, Variable
   required. */
TEST(function_result_passes_the_line_check)
{
    static const char *const lines[] = {
        "10 PRINT sum%(1, 2)",
        "20 FUNCTION sum% (a AS INTEGER, b AS INTEGER)",
        "30 sum% = a + b",
        "40 END FUNCTION",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    uint8_t text[] = "sum% = a + b";
    lexr_line(&S->lexr, text);
    S->comp.check = true;
    (void)comp_line(&S->comp);
    S->comp.check = false;
    const uint8_t *op = S->comp.xbuf;

    while (!exec_is_err(op) && *op != O_EOL)
        op += flow_opsize(op);

    CHECK(*op == O_EOL);
}

/* A fixed array field's element at a field's offset past 64K is one FX.IDX, its offset an operand.
   It took I.LD.I32 and I.ADD besides, +2.7% of HIGHVALE's ops a frame. */
TEST(fixed_array_field_offset_in_its_index)
{
    static const char *const lines[] = {
        "10 DEFINT A-Z",
        "20 TYPE P",
        "30 x AS INTEGER",
        "40 y AS INTEGER",
        "50 END TYPE",
        "60 TYPE G",
        "70 H(16383) AS LONG",
        "80 L(3) AS P",
        "90 END TYPE",
        "100 DIM g AS G",
        "110 i = 2",
        "120 g.L(i).y = 7",
        "130 PRINT g.L(i).y",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 7 \r");
    CHECK(unit_ops(S, O_FX_IDX) == 2);
    CHECK(!unit_ops(S, O_I_ADD) && !unit_ops(S, O_I_ADD_I16) && !unit_ops(S, O_I_LD_I32));
}
