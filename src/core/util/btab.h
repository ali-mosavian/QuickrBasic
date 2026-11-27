/* Block tables' interface. How it works: btab.c. */
#ifndef UTIL_BTAB_H
#define UTIL_BTAB_H
#include <stddef.h>
#include <stdint.h>
#include "util/arena.h"

/* 64 entries a block: 1024 took 56 KB for a program's first procedure. */
enum { BTAB_SHIFT = 6, BTAB_N = 1 << BTAB_SHIFT, BTAB_MASK = BTAB_N - 1 };

typedef struct {
    uint8_t **blocks; /* by block, BTAB_N entries each */
    uint32_t n;       /* the entries: ids 0 .. n-1 */
    uint32_t nblocks; /* the blocks taken */
    uint32_t cap;     /* the room in blocks */
    uint32_t size;    /* an entry's bytes */
} BTab;

/* Empty, for entries of size bytes. Its arena's reset frees what it took. */
static inline void btab_init(BTab *const t, const uint32_t size) { *t = (BTab){.size = size}; }

/* Entry id, below t->n: block id >> BTAB_SHIFT, place id & BTAB_MASK. */
static inline void *btab_at(const BTab *const t, const uint32_t id)
{
    return t->blocks[id >> BTAB_SHIFT] + (size_t)(id & BTAB_MASK) * t->size;
}

/* Entry id of a table of T, as btab_at: T's size a constant, so a 486 takes no multiply. */
#define BTAB_AT(T, t, id) ((T *)(void *)(t)->blocks[(id) >> BTAB_SHIFT] + ((id) & BTAB_MASK))

/* A new entry, zeroed, its id t->n - 1. Null: no memory. */
void *btab_add(BTab *t, Arena *a);
#endif
