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
}
