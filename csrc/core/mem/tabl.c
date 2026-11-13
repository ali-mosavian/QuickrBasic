#include "mem/tabl.h"

static uint32_t row(const Table *t)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < t->narr; i++)
        n += t->size[i];
    return n;
}

void tabl_new(Table *t, uint32_t entries, AlloMoved moved)
{
    t->block = allo_new(entries * row(t), moved);
    t->base = allo_base(t->block);
    t->cap = 0;
    tabl_place(t);
}

bool tabl_room(Table *t, uint32_t entries)
{
    if (entries <= t->cap)
        return true;
    if (t->max < entries)
        return false;
    return allo_grow(t->block, entries * row(t));
}

Addr tabl_place(Table *t)
{
    Addr base = allo_base(t->block);
    uint32_t cap = allo_cap(t->block) / row(t);
    if (cap > t->max)
        cap = t->max;
    /* The block's bytes are at base in the old layout: the arrays after the first move up, the
       last first. */
    for (uint32_t i = t->narr - 1; i > 0; i--) {
        uint32_t before = 0;
        for (uint32_t j = 0; j < i; j++)
            before += t->size[j];
        memmove(at(base + before * cap), at(base + before * t->cap), t->size[i] * t->cap);
    }
    t->cap = cap;
    Addr old = t->base;
    t->base = base;
    for (uint32_t i = 0; i < t->narr; i++) {
        t->at[i] = base;
        base += t->size[i] * cap;
    }
    return old;
}
