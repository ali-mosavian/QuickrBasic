/* A first-fit allocator over one region: the C side's growable buffers on DOS (plat/dos32/pmem.c).
 *
 * # How it works
 * The region is blocks back to back, each a header (its bytes with the header's, in use or free)
 * and its payload. Free blocks join the free ones after them as a walk passes them; a block
 * resized grows into the free ones after it before it moves.
 */
#include "util/heap.h"
#include <string.h>

enum { ALIGN = alignof(max_align_t) };

typedef struct {
    alignas(max_align_t) uint32_t size; /* the block's bytes, its header's too */
    bool used;
} Block;

static size_t rounded(const size_t n) { return (n + ALIGN - 1) & ~(size_t)(ALIGN - 1); }

static Block *next(Block *const b) { return (Block *)((uint8_t *)b + b->size); }

/* b with the free blocks after it. */
static void join(const Heap *const h, Block *const b)
{
    for (Block *n = next(b); (uint8_t *)n < h->hi && !n->used; n = next(b))
        b->size += n->size;
}

/* b to size bytes, its rest a free block when one fits. */
static void split(Block *const b, const size_t size)
{
    if (b->size - size < sizeof(Block) + ALIGN)
        return;

    Block *const rest = (Block *)((uint8_t *)b + size);
    *rest = (Block){.size = b->size - (uint32_t)size};

    b->size = (uint32_t)size;
}

void heap_init(Heap *const h, void *const mem, const size_t size)
{
    uint8_t *const lo = (uint8_t *)rounded((uintptr_t)mem);
    const size_t n = (size_t)((uint8_t *)mem + size - lo) & ~(size_t)(ALIGN - 1);

    *h = (Heap){.lo = lo, .hi = lo + n};

    if (n >= sizeof(Block))
        *(Block *)lo = (Block){.size = (uint32_t)n};
    else
        h->hi = lo;
}

static void *heap_alloc(const Heap *const h, const size_t need)
{
    for (Block *b = (Block *)h->lo; (uint8_t *)b < h->hi; b = next(b)) {
        if (b->used)
            continue;

        join(h, b);

        if (b->size >= need) {
            split(b, need);
            b->used = true;
            return b + 1;
        }
    }

    return nullptr;
}

void *heap_resize(Heap *const h, void *const p, const size_t size)
{
    const size_t need = sizeof(Block) + rounded(size ? size : 1);

    if (!p)
        return heap_alloc(h, need);

    Block *const b = (Block *)p - 1;
    const uint32_t had = b->size;
    join(h, b);

    if (b->size >= need) {
        split(b, need);
        return p;
    }

    split(b, had);
    void *const q = heap_alloc(h, need);

    if (q) {
        memcpy(q, p, had - sizeof(Block));
        b->used = false;
    }

    return q;
}

void heap_free(Heap *const h, void *const p)
{
    if (p)
        ((Block *)p - 1)->used = false;
}
