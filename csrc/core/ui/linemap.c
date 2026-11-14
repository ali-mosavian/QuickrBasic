/* The line map: each editor line's record in the program store.
 *
 * # How it works
 * A block of uint32_t: text line i's record's offset, uimn.text.all of them, rising. An include's
 * records (LF_INC) are no line's.
 *
 * lmap_init takes the block from the heap (main, after the store's, before the symbols').
 * lmap_build rebuilds the map from the store; growing it empties the heap first (symb_clear).
 *
 * # asm structure (src/ui/lmap.asm)
 * | asm | here | |
 * |---|---|---|
 * | lm_init, lm_build | lmap_init, lmap_build | MIRROR |
 * | lm_off | lmap_off | MIRROR |
 * | lm_of | lmap_of | MIRROR; D1 |
 * | HLMAP | HLMAP (mem/alloc.h) | MOVED |
 * | LMINIT | LMINIT (static) | MIRROR |
 *
 * - D1. A flag result is a bool, true the success; the line is returned through a pointer.
 */
#include "ui/linemap.h"
#include "session.h"
#include "io/repl.h"
#include "mem/alloc.h"
#include "mem/program.h"
#include "mem/symbols.h"

enum { LMINIT = 4096 * 4 }; /* the map's first bytes */

static uint32_t *map(Session *const S) { return (uint32_t *)at(allo_base(&S->allo, HLMAP)); }

void lmap_init(Session *const S) { allo_new(S, LMINIT, nullptr); }

void lmap_build(Session *const S)
{
    uint32_t n = 0;

    for (uint32_t rec = PROGBASE; prog_line(&S->prog, rec)->len;

         rec += prog_line(&S->prog, rec)->len) {
        if (prog_line(&S->prog, rec)->flags & LF_INC)
            continue;

        const uint32_t need = (n + 1) * sizeof(uint32_t);

        if (need > allo_cap(&S->allo, HLMAP)) {
            symb_clear(S); /* it grows: the heap emptied first */
            if (!allo_grow(S, HLMAP, need))
                repl_error(S, E_OM);
        }

        map(S)[n++] = rec;
    }

    uimn.text.all = n;
}

uint32_t lmap_off(Session *const S, const uint32_t line)
{
    return line < uimn.text.all ? map(S)[line] : S->prog.end;
}

bool lmap_of(Session *const S, const uint32_t rec, uint32_t *const line)
{
    const uint32_t *const m = map(S);
    uint32_t lo = 0, hi = uimn.text.all; /* [lo, hi): the last <= rec in it */
    if (!hi || rec < m[0])
        return false;
    while (lo + 1 < hi) {
        const uint32_t mid = (lo + hi) / 2;

        if (rec < m[mid])
            hi = mid;
        else
            lo = mid;
    }

    *line = lo;
    return true;
}
