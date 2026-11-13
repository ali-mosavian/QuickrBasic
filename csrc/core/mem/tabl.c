#include "mem/tabl.h"

/* An entry's bytes, all its arrays'. */
static uint32_t row(const Table *const t)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < t->narr; i++)
        n += t->size[i];
    return n;
}

void tabl_new(Table *const t, const uint32_t entries, const AlloMoved moved)
{
    t->block = allo_new(entries * row(t), moved);
    t->base = allo_base(t->block);
    t->cap = 0;
    tabl_place(t);
}

bool tabl_room(Table *const t, const uint32_t entries)
{
    if (entries <= t->cap)
        return true;
    if (t->max < entries)
        return false;
    return allo_grow(t->block, entries * row(t));
}

/* The bytes per entry of the arrays before array i. */
static uint32_t before(const Table *const t, const uint32_t i)
{
    uint32_t n = 0;
    for (uint32_t j = 0; j < i; j++)
        n += t->size[j];
    return n;
}

Addr tabl_place(Table *const t)
{
    const Addr base = allo_base(t->block);
    const uint32_t fit = allo_cap(t->block) / row(t);
    const uint32_t cap = fit < t->max ? fit : t->max;
    /* The block's bytes are at base in the old layout: the arrays after the first move up, the
       last first. */
    for (uint32_t i = t->narr; i-- > 1;)
        memmove(at(base + before(t, i) * cap), at(base + before(t, i) * t->cap), t->size[i] * t->cap);
    const Addr old = t->base;
    t->cap = cap;
    t->base = base;
    for (uint32_t i = 0; i < t->narr; i++)
        t->at[i] = base + before(t, i) * cap;
    return old;
}
