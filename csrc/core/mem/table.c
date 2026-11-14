/* Tables: parallel arrays, an entry of each per id, in one block.
 *
 * # How it works
 * The arrays lie in the block one after another, cap entries each. As the block grows the arrays
 * after the first move up (tabl_moved, its block's owner).
 *
 * # asm structure (src/mem/tab.asm)
 * | asm | here | |
 * |---|---|---|
 * | TTAB | Table (.block .cap .max .base .narr .size[] .at[]); TMAXA | MIRROR |
 * | tab_new (ESI, ECX, EBX) | tabl_new(t, entries, moved) | MIRROR |
 * | tab_room (ESI, ECX; CF) | tabl_room(t, entries) | MIRROR; D1 |
 * | tab_place (ESI -> EDX) | tabl_place(t) | MIRROR; returns the old base |
 *
 * - D1. Returns true when there is room.
 */
#include "mem/table.h"

/* An entry's bytes, all its arrays'. */
static uint32_t tabl_row(const Table *const t)
{
    uint32_t n = 0;

    for (uint32_t i = 0; i < t->narr; i++)
        n += t->size[i];

    return n;
}

static void tabl_moved(AlloOwner *o);

void tabl_new(Allo *const al, Table *const t, const uint32_t entries)
{
    t->owner.moved = tabl_moved;
    t->allo = al;
    t->block = allo_new(al, entries * tabl_row(t), &t->owner);
    t->base = allo_base(al, t->block);
    t->cap = 0;
    tabl_moved(&t->owner);
}

bool tabl_room(Table *const t, const uint32_t entries)
{
    if (entries <= t->cap)
        return true;
    if (t->max < entries)
        return false;

    return allo_grow(t->allo, t->block, entries * tabl_row(t));
}

/* The bytes per entry of the arrays before array i. */
static uint32_t tabl_before(const Table *const t, const uint32_t i)
{
    uint32_t n = 0;

    for (uint32_t j = 0; j < i; j++)
        n += t->size[j];

    return n;
}

/* t where its block now is: its arrays moved up to a larger cap, at set; then its moved. */
static void tabl_moved(AlloOwner *const o)
{
    Table *const t = container_of(o, Table, owner);
    const Addr base = allo_base(t->allo, t->block);
    const uint32_t fit = allo_cap(t->allo, t->block) / tabl_row(t);
    const uint32_t cap = fit < t->max ? fit : t->max;

    /* The block's bytes are at base in the old layout: the arrays after the first move up, the
       last first. */
    for (uint32_t i = t->narr; i-- > 1;)
        memmove(
            at(base + tabl_before(t, i) * cap),
            at(base + tabl_before(t, i) * t->cap),
            t->size[i] * t->cap
        );

    const Addr old = t->base;
    t->cap = cap;
    t->base = base;

    for (uint32_t i = 0; i < t->narr; i++)
        t->at[i] = base + tabl_before(t, i) * cap;

    if (t->moved)
        t->moved(t, old);
}
