/* codegen's golden dump: the file argv[1] loaded and linked, each stored line compiled; per line,
   its index and its code's bytes in hex. A load error: "error n". -f FILE: the code fused, as run;
   -l FILE: as linked, its cells and frames resolved; -d FILE: that, each op by name. A .qr: -l or
   -d only. */
#include <stdio.h>
#include <string.h>
#include "boot.h"
#include "session.h"
#include "comp/link.h"
#include "qr/qr.h"
#include "vm/flow.h"

static Session session;

static void load(Session *const S)
{
    repl_load(S);
    session_drop(S);
}

static void link(Session *const S) { link_start(&S->comp); }

/* A .qr loaded and linked; a compile error, back to the caller. */
static void build(Session *const S)
{
    if (qr_build(S))
        repl_ready(S);
}

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

static const char *const op_names[] = {
#define OP(name, h, size, flags) #name,
#include "vm/ops.def"
};
static const char *const k_names[] = {
#define K(name, h) #name,
#include "vm/ops.def"
};

static bool named; /* -d */

/* The op at b, of size bytes: its name (a CALL's routine's too), then its operands. */
static void dump_op(const uint8_t *const b, const uint32_t size)
{
    printf(" |%s", op_names[*b]);

    if (*b == O_CALL && b[1] < countof(k_names))
        printf(" %s", k_names[b[1]]);

    for (uint32_t i = 1 + (*b == O_CALL); i < size; i++)
        printf(" %02x", b[i]);
}

/* Each stored line's code as linked, in the cache. */
static void dump_linked(Session *const S)
{
    uint32_t n = 0;

    for (uint32_t l = PROGBASE, len; (len = prog_line(&S->prog, l)->len); l += len, n++) {
        printf("%u", n);
        const uint8_t *b = at(S->allo.cache + prog_line(&S->prog, l)->code);
        const uint8_t *end = b;

        while (*end != O_EOL && *end != O_EOL_TO) /* a stored line ends in EOL.TO */
            end += link_opsize(end);

        const uint8_t *const last = end + flow_opsize(end);

        for (uint32_t size; b < last; b += size) {
            size = named ? flow_opsize(b) : 1;

            if (named)
                dump_op(b, size);
            else
                printf(" %02x", *b);
        }

        putchar('\n');
    }
}

int main(const int argc, char **const argv)
{
    const bool fused = argc == 3 && !strcmp(argv[1], "-f");
    named = argc == 3 && !strcmp(argv[1], "-d");
    const bool linked = argc == 3 && (!strcmp(argv[1], "-l") || named);

    if (argc != 2 && !fused && !linked)
        return 2;

    static const FrontEnd none = {};

    Session *const S = &session;
    boot(S, &none);
    S->comp.nofuse = !fused && !linked; /* else the asm's code, byte for byte */
    snprintf(S->repl.file, sizeof S->repl.file, "%s", argv[argc - 1]);

    if (qr_is_file(S->repl.file))
        return !linked ? 2 : repl_call(S, build) != ST_OK || repl_call(S, dump_linked) != ST_OK;

    Status st = repl_call(S, load);

    if (st != ST_OK) {
        printf("error %u\n", st);
        return 0;
    }

    st = repl_call(S, link);

    if (st != ST_OK) /* a link error: each line's code all the same */
        printf("error %u\n", st);
    if ((st = repl_call(S, linked ? dump_linked : dump)) != ST_OK)
        printf("error %u\n", st);

    return 0;
}
