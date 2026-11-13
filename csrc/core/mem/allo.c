#include "mem/allo.h"
#include "io/repl.h"
#include "mem/arry.h"
#include "mem/strg.h"

Allo allo;

/* The control stack's region keeps the asm's tail above CSTOP (g_dsb+FC00h .. 10000h). */
enum { CSTAIL = 0x400, ALIGN = 16 };

static Addr align(const Addr a) { return (a + ALIGN - 1) & ~(Addr)(ALIGN - 1); }

/* Do size more bytes fit at a, HMIN left after them? */
static bool fits(const Addr a, const uint32_t size) { return (uint64_t)a + size + HMIN <= allo.top; }

void allo_init(const Addr lo, const Addr hi)
{
    allo.bot = lo;
    allo.top = hi;
    allo.hdr_base = allo_carve(HDRSZ + CSSZ + CSTAIL);
    allo.cs_lim = allo.hdr_base + HDRSZ;
    allo.cs_top = allo.cs_lim + CSSZ;
    allo.poke = allo_carve(POKESZ);
}

/* The heap's bottom aligned to 16, with size bytes after it and HMIN left; 0 no room. */
static Addr room(const uint32_t size)
{
    const Addr a = align(allo.bot);
    return fits(a, size) ? a : 0;
}

Addr allo_carve(const uint32_t size)
{
    const Addr a = room(size);
    if (!a)
        repl_error(E_OM);
    memset(at(a), 0, size);
    allo.bot = a + size;
    return a;
}

uint32_t allo_new(const uint32_t size, const AlloMoved moved)
{
    const uint32_t h = allo.nblocks;
    const Addr a = room(size);
    if (h >= NBLKH || !a)
        repl_error(E_OM);
    allo.block[h] = (AlloBlock){.base = a, .cap = size, .moved = moved};
    allo.nblocks++;
    allo.bot = a + size;
    return h;
}

static void tell(const AlloBlock *const b)
{
    if (b->moved)
        b->moved();
}

bool allo_grow(const uint32_t h, const uint32_t size)
{
    AlloBlock *const b = &allo.block[h];
    if (size <= b->cap)
        return true;
    const uint32_t half_again = b->cap + b->cap / 2;
    const uint32_t cap = align(half_again < size ? size : half_again);
    if (arry.end != allo.bot || strg.low != allo.top) /* the heap not empty */
        return false;
    const uint32_t grow = cap - b->cap;
    if (!fits(allo.bot, grow))
        return false;
    const Addr end = b->base + b->cap;
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
