/* Errors as values: comp_text's Status. */
#include "session.h"
#include "unit.h"

static Status compiled(Session *const S, const char *const text)
{
    const uint32_t n = (uint32_t)strlen(text);
    memcpy(S->cons.inbuf, text, n);
    CHECK(repl_enter(S, n) == ENTER_DIRECT);
    return comp_line(&S->comp);
}

TEST(compile_error_is_its_status)
{
    CHECK(compiled(S, "x = 1") == ST_OK);
    CHECK(compiled(S, "x = (") == E_SN);
}

/* Folding's catch is its own: 1 / 0 left unfolded, to raise when it runs. */
TEST(fold_error_is_not_the_compiles)
{
    CHECK(compiled(S, "x = 1 / 0") == ST_OK);
    CHECK(compiled(S, "x = 1") == ST_OK);
}
