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
