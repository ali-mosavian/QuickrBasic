/* codegen's golden dump: the file argv[1] loaded and linked, each stored line compiled; per line,
   its index and its code's bytes in hex. A load error: "error n". */
#include <stdio.h>
#include <string.h>
#include "boot.h"
#include "session.h"
#include "comp/link.h"

static Session session;

int main(const int argc, char **const argv)
{
    if (argc != 2)
        return 2;
    static const FrontEnd none = {};
    Session *const S = &session;
    boot(S, &none);
    if (setjmp(S->repl.top)) {
        printf("error %u\n", S->repl.err);
        return 0;
    }
    snprintf(S->repl.file, sizeof S->repl.file, "%s", argv[1]);
    repl_load(S);
    prog_drop(S);
    link_start(S);
    uint32_t n = 0;
    for (uint32_t l = PROGBASE, len; (len = prog_line(&S->prog, l)->len); l += len, n++) {
        link_compile(S, l);
        printf("%u", n);
        for (const uint8_t *b = S->comp.xbuf; b < S->comp.out; b++)
            printf(" %02x", *b);
        putchar('\n');
    }
    return 0;
}
