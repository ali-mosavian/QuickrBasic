/* Linking (comp/link): the whole program compiled before it runs. */
#include <stdio.h>
#include "session.h"
#include "unit.h"

static void enter(Session *const S, const char *const text)
{
    const uint32_t n = (uint32_t)strlen(text);
    memcpy(cons_inroom(&S->cons, n + 1), text, n);
    CHECK(repl_enter(S, n) == ENTER_STORED);
}

/* More code than the cache's first 64 KB: it grows, and no line is left to compile at run time. */
TEST(link_compiles_every_line_growing_the_cache)
{
    char line[INMAX];

    for (uint32_t num = 1; num <= 600; num++) {
        uint32_t n = (uint32_t)snprintf(line, sizeof line, "%u a = 1", num);

        for (uint32_t k = 0; k < 20; k++)
            n += (uint32_t)snprintf(line + n, sizeof line - n, ": a = a * 3 + %u", k);

        enter(S, line);
    }

    link_start(&S->comp);
    CHECK(allo_cap(&S->allo, HCACHE) > 0x10000);

    for (uint32_t l = PROGBASE, len; (len = prog_line(&S->prog, l)->len); l += len)
        CHECK(prog_line(&S->prog, l)->code);

    session_new(S);
}
