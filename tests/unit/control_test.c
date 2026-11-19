/* Blocks compiled (comp/control): the IDE's check of a line alone. */
#include "session.h"
#include "unit.h"

static bool erred;

static void each(const uint8_t op, [[maybe_unused]] const uint8_t *const at)
{
    erred |= op == O_ERR;
}

/* The direct line text compiled as the editor checks a line: an error in its code? */
static bool check_errs(Session *const S, const char *const text)
{
    const uint32_t n = (uint32_t)strlen(text);
    memcpy(S->cons.inbuf, text, n);
    CHECK(repl_enter(S, n) == ENTER_DIRECT);
    S->comp.check = true;
    comp_line(&S->comp);
    S->comp.check = false;
    erred = false;
    link_walk(S->comp.xbuf, each);
    return erred;
}

/* A NEXT typed on its own line was checked as NEXT without FOR: the editor's error dialog. */
TEST(check_passes_next_alone)
{
    CHECK(!check_errs(S, "next"));
    CHECK(!check_errs(S, "next i"));
    CHECK(!check_errs(S, "next i, j"));
}

/* A NEXT alone was skipped to its statement's end: NEXT IF and NEXT I PRINT passed the check. */
TEST(check_reads_next_alone)
{
    CHECK(check_errs(S, "next if"));
    CHECK(check_errs(S, "next i print"));
}

/* DEFINT on typed lines: the events were a file's load's only, so a typed program's variables
   stayed SINGLE: X = 1.7 printed 1.7, and passed to an INTEGER parameter, Parameter type
   mismatch. */
TEST(defint_on_typed_lines)
{
    static const char *const lines[] = {
        "10 DEFINT A-Z",
        "20 x = 1.7: PRINT x; sum%(x, x);",
        "30 FUNCTION sum% (a AS INTEGER, b AS INTEGER)",
        "40 sum% = a + b",
        "50 END FUNCTION",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 2  4 ");
}

/* A DEFtype line typed last, put first by its number, holds from its place. */
TEST(defint_typed_after)
{
    static const char *const lines[] = {"20 x = 1.7: PRINT x;", "10 DEFINT A-Z", nullptr};

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 2 ");
}

/* A statement whose arguments may be left out ended only at the line's end: CLOSE:, CLOSE ' and
   CLOSE ELSE were Syntax errors, the rest of the line not run. */
TEST(an_argless_statement_ends_at_a_colon)
{
    static const char *const lines[] = {
        "10 ON ERROR GOTO 90",
        "20 CLOSE: PRINT 1;: CLOSE : PRINT 2;",
        "30 CLOSE ' all",
        "40 IF 1 THEN CLOSE ELSE PRINT 9;",
        "50 IF 0 THEN PRINT 9; ELSE CLOSE: PRINT 3;",
        "60 RANDOMIZE 1: PRINT 4;: CLOSE #1, 2: PRINT 5;",
        "70 END",
        "90 PRINT \"e\"; ERR;: RESUME NEXT",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 1  2  3  4  5 ");
}
/* The status of lines run with SELECT i's body given (its END SELECT after). */
static Status select_body(Session *const S, const char *const a, const char *const b)
{
    const char *const lines[] = {"i = 1", "SELECT CASE i", a, b, "END SELECT", nullptr};

    return unit_run_file(S, lines);
}

/* A statement, label or line number between SELECT CASE and its first CASE compiled, a statement
   never run; QBasic's and BC's: Statements/labels illegal between SELECT CASE and CASE. A blank
   line, ':' or a comment may be there. */
TEST(nothing_between_select_and_case)
{
    CHECK(select_body(S, "PRINT 9;", "CASE 1") == E_SLC);
    CHECK(select_body(S, "DATA 1", "CASE 1") == E_SLC);
    CHECK(select_body(S, "here: CASE 1", "CASE 2") == E_SLC);
    CHECK(select_body(S, "100 CASE 1", "CASE 2") == E_SLC);

    static const char *const same[] =
        {"i = 1", "SELECT CASE i: PRINT 9;", "CASE 1", "END SELECT", nullptr};

    CHECK(unit_run_file(S, same) == E_SLC);

    CHECK(select_body(S, "' a comment", "CASE 1: PRINT 1;") == ST_OK);
    CHECK(select_body(S, "REM x: CASE 9", ":") == ST_OK);

    static const char *const ok[] = {
        "i = 1",
        "SELECT CASE i: CASE 1: PRINT 1;: END SELECT",
        "SELECT CASE i",
        "END SELECT",
        nullptr
    };

    CHECK(unit_run_file(S, ok) == ST_OK);
}

/* Lines typed into the editor one by one, each declared then checked alone as edit_canon does:
   the last one's check an error? */
static bool typed_errs(Session *const S, const char *const *const lines)
{
    for (const char *const *l = lines; *l; l++) {
        uint8_t text[256];
        memcpy(text, *l, strlen(*l) + 1);
        S->vm.line = DLINE;
        lexr_line(&S->lexr, text, S->comp.cbuf);
        repl_declare(S, S->comp.cbuf);
        S->comp.check = true;
        comp_line(&S->comp);
        S->comp.check = false;
        erred = false;
        link_walk(S->comp.xbuf, each);
    }

    return erred;
}

/* A string CASE checked alone took its SELECT for a number's: GORILLA.BAS's CASE "." and
   CASE CHR$(13) opened as Type mismatch, kept as typed. */
TEST(check_passes_string_case_alone)
{
    CHECK(!check_errs(S, "case \".\""));
    CHECK(!check_errs(S, "case \"0\" to \"9\""));
    CHECK(!check_errs(S, "case chr$(13), \"a\""));
    CHECK(!check_errs(S, "case is < \"m\""));
    CHECK(!check_errs(S, "case 1 to 9"));
    CHECK(check_errs(S, "case \"a\", 1"));
}

/* A record array's parameter's element's field, checked alone in its SUB: GORILLA.BAS's
   BCoor(i).XCoor = x opened as Syntax error, kept as typed. */
TEST(check_passes_record_parameter_field)
{
    static const char *const lines[] = {
        "TYPE XYPoint",
        "XCoor AS INTEGER",
        "YCoor AS INTEGER",
        "END TYPE",
        "SUB MakeCityScape (BCoor() AS XYPoint)",
        "BCoor(i).XCoor = x",
        nullptr,
    };

    CHECK(!typed_errs(S, lines));
}

/* A record passed whole to a FUNCTION not yet linked, checked alone: taken for a value, Type
   mismatch at its ')'. CHARLIE.BAS's IF Wall%(p, ...) and IF Land%(p, d) were kept as typed. */
TEST(check_passes_record_argument_whole)
{
    static const char *const lines[] = {
        "TYPE Hero",
        "x AS INTEGER",
        "END TYPE",
        "DECLARE FUNCTION W% (p AS Hero)",
        "SUB S (p AS Hero)",
        "IF W%(p) THEN PRINT 1",
        nullptr,
    };

    CHECK(!typed_errs(S, lines));
}

/* A FUNCTION AS a TYPE, ours, and its calls, typed one line at a time before any link: a = Mk(2)
   and Mk = Mk(1) were Type mismatch, the FUNCTION's result unknown there. */
TEST(check_passes_record_result)
{
    static const char *const lines[] = {
        "TYPE Vec",
        "x AS SINGLE",
        "END TYPE",
        "DECLARE FUNCTION Mk (x!) AS Vec",
        "DIM a AS Vec",
        "a = Mk(2)",
        "PRINT Mk(2).x",
        "FUNCTION Mk (x!) AS Vec",
        "Mk.x = x!",
        "Mk = Mk(1)",
        nullptr,
    };
    const char *typed[countof(lines)] = {};

    for (size_t n = 0; lines[n]; n++) {
        typed[n] = lines[n];
        CHECK(!typed_errs(S, typed));
    }
}

/* A FUNCTION AS an array, ours, its calls and its result's elements, typed one line at a time
   before any link: s() = Sq(4) was a Syntax error at its 4. */
TEST(check_passes_array_result)
{
    static const char *const lines[] = {
        "DECLARE FUNCTION Sq (n AS INTEGER) AS LONG()",
        "DECLARE SUB Show (a() AS LONG)",
        "DIM s() AS LONG",
        "s() = Sq(4)",
        "Show Sq(2)",
        "FUNCTION Sq (n AS INTEGER) AS LONG()",
        "REDIM Sq(1 TO n)",
        "Sq(1) = 5",
        "x = Sq(1) + UBOUND(Sq)",
        nullptr,
    };
    const char *typed[countof(lines)] = {};

    for (size_t n = 0; lines[n]; n++) {
        typed[n] = lines[n];
        CHECK(!typed_errs(S, typed));
    }
}
