#include "mem/allo.h"
#include "session.h"
#include "io/repl.h"
#include "mem/arry.h"
#include "mem/strg.h"


/* The control stack's region keeps the asm's tail above CSTOP (g_dsb+FC00h .. 10000h). */
enum { CSTAIL = 0x400, ALIGN = 16 };

static Addr align(const Addr a) { return (a + ALIGN - 1) & ~(Addr)(ALIGN - 1); }

/* Do size more bytes fit at a, HMIN left after them? */
static bool fits(Session *const S, const Addr a, const uint32_t size) { return (uint64_t)a + size + HMIN <= S->allo.top; }

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
