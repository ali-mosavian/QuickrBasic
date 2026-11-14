/* The VM's inline ops (vm/hot.h) as the ops table's. */
#include "session.h"
#include "unit.h"

/* text entered as a typed line. */
static uint8_t entered(Session *const S, const char *const text)
{
    const uint32_t n = (uint32_t)strlen(text);
    memcpy(S->cons.inbuf, text, n);
    return repl_enter(S, n);
}

static void direct(Session *const S) { repl_run(S); }

/* The program's lines entered (no other test stores lines) and RUN; its output, in S->cons.sbuf. */
static Status ran(Session *const S, const char *const *const lines)
{
    for (size_t i = 0; lines[i]; i++)
        CHECK(entered(S, lines[i]) == ENTER_STORED);

    CHECK(entered(S, "RUN") == ENTER_DIRECT);
    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    const Status st = repl_call(S, direct);
    S->cons.capture = 0;
    return st;
}

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

    CHECK(ran(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 6 nextend");
}
