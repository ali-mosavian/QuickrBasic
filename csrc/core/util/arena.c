#include "util/arena.h"
#include <string.h>
#include "plat/pmem.h"

enum { ALIGN = alignof(max_align_t) };

struct ArenaBlock {
    ArenaBlock *next;
    uint8_t *end;
    alignas(max_align_t) uint8_t mem[];
};

static size_t rounded(const size_t n) { return (n + ALIGN - 1) & ~(size_t)(ALIGN - 1); }

void arena_init(Arena *const a, const size_t chunk) { *a = (Arena){.chunk = chunk}; }

/* The block after cur with room for size: a kept one, else a new one from the platform. */
static bool next_block(Arena *const a, const size_t size)
{
    ArenaBlock *b = a->cur ? a->cur->next : a->first;

    for (; b; b = b->next)
        if ((size_t)(b->end - b->mem) >= size)
            break;
    if (!b) {
        const size_t room = size > a->chunk ? size : a->chunk;
        b = pmem_block(sizeof *b + room);

        if (!b)
            return false;

        b->end = b->mem + room;
        b->next = nullptr;

        if (a->cur) { /* after cur: the blocks it skipped stay later in the list */
            b->next = a->cur->next;
            a->cur->next = b;
        } else {
            b->next = a->first;
            a->first = b;
        }
    }

    a->cur = b;
    a->top = b->mem;
    return true;
}

void *arena_alloc(Arena *const a, const size_t size)
{
    const size_t n = rounded(size);

    if ((!a->cur || (size_t)(a->cur->end - a->top) < n) && !next_block(a, n))
        return nullptr;

    void *const p = a->top;
    a->top += n;
    memset(p, 0, size);
    return p;
}

void *arena_grow(Arena *const a, void *const old, const size_t size, const size_t newsize)
{
    uint8_t *const o = old;

    if (o && o + rounded(size) == a->top && (size_t)(a->cur->end - o) >= rounded(newsize)) {
        a->top = o + rounded(newsize);
        memset(o + size, 0, newsize - size);
        return o;
    }

    uint8_t *const p = arena_alloc(a, newsize);

    if (p && o)
        memcpy(p, o, size);

    return p;
}

void arena_reset(Arena *const a)
{
    a->cur = a->first;
    a->top = a->first ? a->first->mem : nullptr;
}
