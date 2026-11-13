/* The line map (src/ui/lmap.asm). */
#include "ui/lmap.h"
#include "io/repl.h"
#include "mem/allo.h"
#include "mem/prog.h"
#include "mem/symb.h"

enum { LMINIT = 4096 * 4 }; /* the map's first bytes */

static uint32_t *map(void) { return (uint32_t *)at(allo_base(HLMAP)); }

void lmap_init(void) { allo_new(LMINIT, nullptr); }

void lmap_build(void)
{
    uint32_t n = 0;
    for (uint32_t rec = PROGBASE; prog_line(rec)->len; rec += prog_line(rec)->len) {
        if (prog_line(rec)->flags & LF_INC)
            continue;
        if (n * 4 + 4 > allo_cap(HLMAP)) {
            symb_clear(); /* it grows: the heap emptied first */
            if (!allo_grow(HLMAP, n * 4 + 4))
                repl_error(E_OM);
        }
        map()[n++] = rec;
    }
    uimn.text.all = n;
}

uint32_t lmap_off(uint32_t line)
{
    return line < uimn.text.all ? map()[line] : prog.end;
}

bool lmap_of(uint32_t rec, uint32_t *line)
{
    const uint32_t *m = map();
    uint32_t lo = 0, hi = uimn.text.all; /* [lo, hi): the last <= rec in it */
    if (!hi || rec < m[0])
        return false;
    while (lo + 1 < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (rec < m[mid])
            hi = mid;
        else
            lo = mid;
    }
    *line = lo;
    return true;
}
