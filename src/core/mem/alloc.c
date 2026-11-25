/* The arena's layout and the allocator: fixed regions and blocks packed from the heap's start.
 *
 * # How it works
 * The arena, [lo, hi) from pmem_init, is carved at start: first the fixed regions (allo_carve: the
 * control stack, the POKE memory, and any other module's region a stored Addr can reach), then the
 * blocks (allo_new: the code cache, the program store, the editor's line map, the symbol tables),
 * then the BASIC heap, bot to top: arrays up from bot (allo.arrays), strings
 * down from top (allo.strings).
 *
 * A block is reached by its handle, never by a stored Addr: it moves when one before it grows.
 * allo_grow moves the blocks after it up and tells each one's moved routine.
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
 * | plt_heap's hbot/htop | pmem_init, then allo_init(lo, hi) | MIRROR |
 * | g_dsb+0BC00h HDRBASE, HDRLIM | OLDHDRSZ, unused | MOVED: mem/array.c's heap blocks |
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

/* The control stack's region keeps the asm's tail above CSTOP (g_dsb+FC00h .. 10000h). */
enum { CSTAIL = 0x400, ALIGN = 16 };

static Addr allo_align(const Addr a) { return (a + ALIGN - 1) & ~(Addr)(ALIGN - 1); }

/* Do size more bytes fit at a, HMIN left after them? */
static bool allo_fits(const Allo *const al, const Addr a, const uint32_t size)
{
    return (uint64_t)a + size + HMIN <= al->top;
}

void allo_init(Allo *const al, Fault *const f, const Addr lo, const Addr hi)
{
    al->fault = f;
    al->bot = lo;
    al->top = hi;
    al->cs_lim = allo_carve(al, OLDHDRSZ + CSSZ + CSTAIL) + OLDHDRSZ;
    al->cs_top = al->cs_lim + CSSZ;
    al->poke = allo_carve(al, POKESZ);
}

/* The heap's bottom aligned to 16, with size bytes after it and HMIN left; 0 no room. */
static Addr allo_room(const Allo *const al, const uint32_t size)
{
    const Addr a = allo_align(al->bot);
    return allo_fits(al, a, size) ? a : 0;
}

Addr allo_carve(Allo *const al, const uint32_t size)
{
    const Addr a = allo_room(al, size);

    if (!a)
        fault_error(al->fault, E_OM);

    memset(at(a), 0, size);
    al->bot = a + size;
    return a;
}

uint32_t allo_new(Allo *const al, const uint32_t size, AlloOwner *const owner)
{
    const uint32_t h = al->nblocks;
    const Addr a = allo_room(al, size);

    if (h >= NBLKH || !a)
        fault_error(al->fault, E_OM);

    al->block[h] = (AlloBlock){.base = a, .cap = size, .owner = owner};

    al->nblocks++;
    al->bot = a + size;
    return h;
}

static void allo_tell(const AlloBlock *const b)
{
    if (b->owner)
        b->owner->moved(b->owner);
}

bool allo_grow(Allo *const al, const uint32_t h, const uint32_t size)
{
    AlloBlock *const b = &al->block[h];

    if (size <= b->cap)
        return true;

    const uint32_t half_again = b->cap + b->cap / 2;
    const uint32_t cap = allo_align(half_again < size ? size : half_again);

    if (al->arrays != al->bot || al->strings != al->top) /* the heap not empty */
        return false;

    const uint32_t grow = cap - b->cap;

    if (!allo_fits(al, al->bot, grow))
        return false;

    const Addr end = b->base + b->cap;
    b->cap = cap;
    memmove(at(end + grow), at(end), al->bot - end);
    al->bot += grow;
    al->arrays = al->bot;
    allo_tell(b);

    for (uint32_t i = h + 1; i < al->nblocks; i++) {
        al->block[i].base += grow;
        allo_tell(&al->block[i]);
    }

    return true;
}
