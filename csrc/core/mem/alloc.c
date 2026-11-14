/* The arena's layout and the allocator: fixed regions and blocks packed from the heap's start.
 *
 * # How it works
 * The arena, [lo, hi) from pmem_init, is carved at start: first the fixed regions (allo_carve: the
 * arrays' headers and the control stack, the POKE memory, and any other module's region a stored
 * Addr can reach), then the blocks (allo_new: the code cache, the program store, the editor's line
 * map, the symbol tables), then the BASIC heap, bot to top: arrays up from bot (arry.end), strings
 * down from top (strg.low).
 *
 * A block is reached by its handle, never by a stored Addr: it moves when one before it grows.
 * allo_grow moves the blocks after it up and tells each one's moved routine.
 *
 * # asm structure (src/mem/alloc.asm, src/regions.asm)
 * | asm | here | |
 * |---|---|---|
 * | TALLOC, g_state.alloc.tab[h] | AlloBlock, Allo.block[h] (.base .cap .moved) | MIRROR |
 * | alloc.nb | Allo.nblocks | MIRROR |
 * | NBLKH HCACHE HPROG HLMAP | same | MIRROR |
 * | mm.hbot, mm.htop | Allo.bot, Allo.top | MIRROR |
 * | mm.cachebase, cblea, cbcmp | Allo.cache | MIRROR |
 * | alloc_new (ECX, EBX -> EAX, EDX) | allo_new(size, moved); base: allo_base(h) | MIRROR; D1 |
 * | alloc_grow (EAX, ECX; CF) | allo_grow(h, size) | MIRROR; D2 |
 * | alloc_sel, sel_set, mm.peekseg | | D1 |
 * | plt_heap's hbot/htop | pmem_init, then allo_init(lo, hi) | MIRROR |
 * | g_dsb+0BC00h HDRBASE, HDRLIM | Allo.hdr_base, Allo.hdr_base + HDRSZ | MIRROR |
 * | CSLIM, CSTOP | Allo.cs_lim, Allo.cs_top | MIRROR |
 * | g_pokeb | Allo.poke (POKESZ) | MIRROR |
 * | g_tmpd, sg.tmplin | Strg.tmp | MOVED: strg_init (mem/string.c) |
 * | g_stkb, g_lsav, g_inbuf, g_clpb | | MOVED: each owner's init carves its own |
 * | alloc_new for the cache and the program store, prg_place | | MOVED: boot (boot.c) |
 * | | allo_init, allo_carve | D3 |
 *
 * - D1. Blocks are reached by handle and base: no selectors (alloc_sel, sel_set); the peek segment
 *   (always 0) is dropped.
 * - D2. allo_grow returns true when grown, where the asm's CF is clear.
 * - D3. New: allo_init sets the heap and carves the fixed regions; allo_carve is for any other
 *   module's.
 */
#include "mem/alloc.h"
#include "session.h"
#include "io/repl.h"
#include "mem/array.h"
#include "mem/string.h"

/* The control stack's region keeps the asm's tail above CSTOP (g_dsb+FC00h .. 10000h). */
enum { CSTAIL = 0x400, ALIGN = 16 };

static Addr align(const Addr a) { return (a + ALIGN - 1) & ~(Addr)(ALIGN - 1); }

/* Do size more bytes fit at a, HMIN left after them? */
static bool fits(Session *const S, const Addr a, const uint32_t size)
{
    return (uint64_t)a + size + HMIN <= S->allo.top;
}

void allo_init(Session *const S, const Addr lo, const Addr hi)
{
    S->allo.bot = lo;
    S->allo.top = hi;
    S->allo.hdr_base = allo_carve(S, HDRSZ + CSSZ + CSTAIL);
    S->allo.cs_lim = S->allo.hdr_base + HDRSZ;
    S->allo.cs_top = S->allo.cs_lim + CSSZ;
    S->allo.poke = allo_carve(S, POKESZ);
}

/* The heap's bottom aligned to 16, with size bytes after it and HMIN left; 0 no room. */
static Addr room(Session *const S, const uint32_t size)
{
    const Addr a = align(S->allo.bot);
    return fits(S, a, size) ? a : 0;
}

Addr allo_carve(Session *const S, const uint32_t size)
{
    const Addr a = room(S, size);

    if (!a)
        repl_error(S, E_OM);

    memset(at(a), 0, size);
    S->allo.bot = a + size;
    return a;
}

uint32_t allo_new(Session *const S, const uint32_t size, const AlloMoved moved)
{
    const uint32_t h = S->allo.nblocks;
    const Addr a = room(S, size);

    if (h >= NBLKH || !a)
        repl_error(S, E_OM);

    S->allo.block[h] = (AlloBlock){.base = a, .cap = size, .moved = moved};

    S->allo.nblocks++;
    S->allo.bot = a + size;
    return h;
}

static void tell(Session *const S, const AlloBlock *const b)
{
    if (b->moved)
        b->moved(S);
}

bool allo_grow(Session *const S, const uint32_t h, const uint32_t size)
{
    AlloBlock *const b = &S->allo.block[h];

    if (size <= b->cap)
        return true;

    const uint32_t half_again = b->cap + b->cap / 2;
    const uint32_t cap = align(half_again < size ? size : half_again);

    if (S->arry.end != S->allo.bot || S->strg.low != S->allo.top) /* the heap not empty */
        return false;

    const uint32_t grow = cap - b->cap;

    if (!fits(S, S->allo.bot, grow))
        return false;

    const Addr end = b->base + b->cap;
    b->cap = cap;
    memmove(at(end + grow), at(end), S->allo.bot - end);
    S->allo.bot += grow;
    S->arry.end = S->allo.bot;
    tell(S, b);

    for (uint32_t i = h + 1; i < S->allo.nblocks; i++) {
        S->allo.block[i].base += grow;
        tell(S, &S->allo.block[i]);
    }

    return true;
}
