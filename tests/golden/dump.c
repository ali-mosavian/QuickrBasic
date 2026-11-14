/* codegen's golden dump: the file argv[1] loaded and linked, each stored line compiled; per line,
   its index and its code's bytes in hex. A load error: "error n". */
#include <stdio.h>
#include <string.h>
#include "boot.h"
#include "session.h"
#include "comp/link.h"

static Session session;

static void load(Session *const S)
{
    repl_load(S);
    session_drop(S);
}

static void link(Session *const S) { link_start(&S->comp); }

/* Each stored line compiled: its index, its code's bytes. */
static void dump(Session *const S)
{
    uint32_t n = 0;

    for (uint32_t l = PROGBASE, len; (len = prog_line(&S->prog, l)->len); l += len, n++) {
        link_compile(&S->comp, l);
        printf("%u", n);

        for (const uint8_t *b = S->comp.xbuf; b < S->code.out; b++)
            printf(" %02x", *b);

        putchar('\n');
    }
}

int main(const int argc, char **const argv)
{
    if (argc != 2)
        return 2;

    static const FrontEnd none = {};

    Session *const S = &session;
    boot(S, &none);
    snprintf(S->repl.file, sizeof S->repl.file, "%s", argv[1]);
    Status st = repl_call(S, load);

    if (st != ST_OK) {
        printf("error %u\n", st);
        return 0;
    }

    st = repl_call(S, link);

    if (st != ST_OK) /* a link error: each line's code all the same */
        printf("error %u\n", st);
    if ((st = repl_call(S, dump)) != ST_OK)
        printf("error %u\n", st);

    return 0;
}
