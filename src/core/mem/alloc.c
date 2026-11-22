/* The arena: fixed regions, blocks and the BASIC heap, all pmem's.
 *
 * # How it works
 * The BASIC heap is a block at the bottom of pmem's largest room: arrays up from bot (allo.arrays),
 * strings down from top (allo.strings). It grows up in place into the room after it, the strings
 * slid up into what it gained (mem/string.c): nothing else in it moves, and a statement may hold
 * an array's Addr. Empty (session_clear), it shrinks back to HEAP_FIRST bytes.
 *
 * A fixed region (allo_carve: the arrays' headers and the control stack, the POKE memory, and any
 * other module's region a stored Addr can reach) is a pmem block that never moves. A block (allo_new:
 * the code cache, the program store, the editor's line map, the symbol tables) is one that may:
 * it is reached by its handle, never by a stored Addr, and allo_grow tells its owner when it moves.
 *
 * # asm structure (retired/asm32/mem/alloc.asm, retired/asm32/regions.asm)
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
 * | plt_heap's hbot/htop | pmem_init, then allo_init | MIRROR |
 * | g_dsb+0BC00h HDRBASE, HDRLIM | Allo.hdr_base, Allo.hdr_base + HDRSZ | MIRROR |
 * | CSLIM, CSTOP | Allo.cs_lim, Allo.cs_top | MIRROR |
 * | g_pokeb | Allo.poke (POKESZ), taken at the first SYS | MIRROR |
 * | g_tmpd, sg.tmplin | Strg.tmp | MOVED: strg_init (mem/string.c) |
 * | g_stkb, g_lsav, g_inbuf, g_clpb | | MOVED: each owner's init carves its own |
 * | alloc_new for the cache and the program store, prg_place | | MOVED: boot (boot.c) |
 * | | allo_init, allo_carve | D3 |
 *
 * - D1. Blocks are reached by handle and base: no selectors (alloc_sel, sel_set); the peek segment
 *   (always 0) is dropped.
 * - D2. allo_grow returns true when grown, where the asm's CF is clear.
 * - D3. New: allo_init sets the heap and carves the fixed regions; allo_carve is for any other
 *   module's. Each is pmem's (plat/pmem.h), as the heap is.
 */
#include "mem/alloc.h"
#include "plat/pmem.h"

/* The control stack's region keeps the asm's tail above CSTOP (g_dsb+FC00h .. 10000h). */
enum { CSTAIL = 0x400, ALIGN = 16 };

static uint32_t allo_align(const uint32_t n) { return (n + ALIGN - 1) & ~(uint32_t)(ALIGN - 1); }

void allo_init(Allo *const al, Fault *const f)
{
    al->fault = f;
    uint8_t *const heap = pmem_bottom(HEAP_FIRST);

    if (!heap)
        fault_error(f, E_OM);

    al->bot = al->arrays = addr_of(heap);
    al->top = al->strings = al->bot + HEAP_FIRST;
    al->hdr_base = allo_carve(al, HDRSZ + CSSZ + CSTAIL);
    al->cs_lim = al->hdr_base + HDRSZ;
    al->cs_top = al->cs_lim + CSSZ;
}

Addr allo_carve(Allo *const al, const uint32_t size)
{
    void *const p = pmem_resize(nullptr, size);

    if (!p)
        fault_error(al->fault, E_OM);

    memset(p, 0, size);
    return addr_of(p);
}

uint32_t allo_new(Allo *const al, const uint32_t size, AlloOwner *const owner)
{
    const uint32_t h = al->nblocks;

    if (h >= NBLKH)
        fault_error(al->fault, E_OM);

    al->block[h] = (AlloBlock){.base = allo_carve(al, size), .cap = size, .owner = owner};
    al->nblocks++;
    return h;
}

bool allo_grow(Allo *const al, const uint32_t h, const uint32_t size)
{
    AlloBlock *const b = &al->block[h];

    if (size <= b->cap)
        return true;

    const uint32_t half_again = b->cap + b->cap / 2;
    uint32_t cap = allo_align(half_again < size ? size : half_again);
    uint8_t *p = pmem_resize(at(b->base), cap);

    if (!p && cap > allo_align(size)) /* short of memory: what was asked */
        p = pmem_resize(at(b->base), cap = allo_align(size));
    if (!p)
        return false;

    memset(p + b->cap, 0, cap - b->cap);
    const bool moved = addr_of(p) != b->base;
    b->base = addr_of(p);
    b->cap = cap;

    if (h == HCACHE)
        al->cache = b->base;
    if (moved && b->owner)
        b->owner->moved(b->owner);

    return true;
}

static uint32_t allo_more(const Allo *const al) { return (uint32_t)pmem_after(at(al->bot)); }

uint32_t allo_free(const Allo *const al) { return al->strings - al->arrays + allo_more(al); }

uint32_t allo_extend(Allo *const al, const uint32_t need)
{
    const uint32_t more = allo_more(al), want = allo_align(need);

    if (more < want)
        return 0;

    const uint32_t quarter = allo_align((al->top - al->bot) / 4);
    const uint32_t add = want < quarter ? (quarter < more ? quarter : more) : want;

    if (!pmem_extend(at(al->bot), al->top - al->bot + add))
        return 0;

    return add;
}

void allo_shrink(Allo *const al)
{
    pmem_extend(at(al->bot), HEAP_FIRST); /* smaller: in place */
    al->arrays = al->bot;
    al->top = al->strings = al->bot + HEAP_FIRST;
}
