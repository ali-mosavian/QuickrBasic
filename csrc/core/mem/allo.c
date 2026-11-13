#include "mem/allo.h"
#include "io/repl.h"
#include "mem/arry.h"
#include "mem/strg.h"

Allo allo;

/* The control stack's region keeps the asm's tail above CSTOP (g_dsb+FC00h .. 10000h). */
enum { CSTAIL = 0x400 };

void allo_init(Addr lo, Addr hi)
{
    allo.bot = lo;
    allo.top = hi;
    allo.hdr_base = allo_carve(HDRSZ + CSSZ + CSTAIL);
    allo.cs_lim = allo.hdr_base + HDRSZ;
    allo.cs_top = allo.cs_lim + CSSZ;
    allo.poke = allo_carve(POKESZ);
}

/* The heap's bottom aligned to 16, with size bytes after it and HMIN left; 0 no room. */
static Addr room(uint32_t size)
{
    Addr a = (allo.bot + 15) & ~15u;
    if ((uint64_t)a + size + HMIN > allo.top)
        return 0;
    return a;
}

Addr allo_carve(uint32_t size)
{
    Addr a = room(size);
    if (!a)
        repl_error(E_OM);
    memset(at(a), 0, size);
    allo.bot = a + size;
    return a;
}

uint32_t allo_new(uint32_t size, AlloMoved moved)
{
    uint32_t h = allo.nblocks;
    Addr a = room(size);
    if (h >= NBLKH || !a)
        repl_error(E_OM);
    allo.block[h] = (AlloBlock){a, size, moved};
    allo.nblocks++;
    allo.bot = a + size;
    return h;
}

static void tell(const AlloBlock *b)
{
    if (b->moved)
        b->moved();
}

bool allo_grow(uint32_t h, uint32_t size)
{
    AlloBlock *b = &allo.block[h];
    if (size <= b->cap)
        return true;
    uint32_t cap = b->cap + b->cap / 2;
    if (cap < size)
        cap = size;
    cap = (cap + 15) & ~15u;
    if (arry.end != allo.bot || strg.low != allo.top) /* the heap not empty */
        return false;
    uint32_t grow = cap - b->cap;
    if ((uint64_t)allo.bot + grow + HMIN > allo.top)
        return false;
    Addr end = b->base + b->cap;
    b->cap = cap;
    memmove(at(end + grow), at(end), allo.bot - end);
    allo.bot += grow;
    arry.end = allo.bot;
    tell(b);
    for (uint32_t i = h + 1; i < allo.nblocks; i++) {
        allo.block[i].base += grow;
        tell(&allo.block[i]);
    }
    return true;
}
