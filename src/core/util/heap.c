/* One heap over several regions: everything the C side, the runtime and the arena take (plat's
 * pmem.c).
 *
 * # How it works
 * A region is blocks back to back, each a header (its bytes with the header's, in use or free)
 * and its payload. Free blocks join the free ones after them as a walk passes them; a block
 * resized grows into the free ones after it before it moves.
 *
 * A new block goes to the top of the first region with room, heap_bottom's block's (the BASIC
 * heap's) last: that block grows up into its region in place, and what the others hold is left it.
 */
#include "util/heap.h"
#include <string.h>

enum { ALIGN = 16 }; /* the arena's blocks, the stack's segments */

typedef struct {
    alignas(ALIGN) uint32_t size; /* the block's bytes, its header's too */
    bool used;
} Block;

static size_t rounded(const size_t n) { return (n + ALIGN - 1) & ~(size_t)(ALIGN - 1); }

static size_t needed(const size_t size) { return sizeof(Block) + rounded(size ? size : 1); }

static Block *next(Block *const b) { return (Block *)((uint8_t *)b + b->size); }

/* b with the free blocks after it, in region r. */
static void join(const HeapRegion *const r, Block *const b)
{
    for (Block *n = next(b); (uint8_t *)n < r->hi && !n->used; n = next(b))
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

void heap_init(Heap *const h) { *h = (Heap){.grow = HEAP_REGIONS}; }

bool heap_add(Heap *const h, void *const mem, const size_t size)
{
    uint8_t *const lo = (uint8_t *)rounded((uintptr_t)mem);
    const size_t n = (uint8_t *)mem + size > lo
                         ? (size_t)((uint8_t *)mem + size - lo) & ~(size_t)(ALIGN - 1)
                         : 0;

    if (h->n == HEAP_REGIONS)
        return false;

    if (n < sizeof(Block) + ALIGN)
        return true;

    h->region[h->n++] = (HeapRegion){.lo = lo, .hi = lo + n};

    *(Block *)lo = (Block){.size = (uint32_t)n};

    return true;
}

static const HeapRegion *region_of(const Heap *const h, const void *const p)
{
    for (uint32_t i = 0; i < h->n; i++)
        if ((const uint8_t *)p > h->region[i].lo && (const uint8_t *)p < h->region[i].hi)
            return &h->region[i];

    return nullptr;
}

/* need bytes at the top of region r's last free block with room; null none. */
static void *take_high(const HeapRegion *const r, const size_t need)
{
    Block *found = nullptr;

    for (Block *b = (Block *)r->lo; (uint8_t *)b < r->hi; b = next(b)) {
        if (b->used)
            continue;

        join(r, b);

        if (b->size >= need)
            found = b;
    }

    if (!found)
        return nullptr;

    if (found->size - need < sizeof(Block) + ALIGN) {
        found->used = true;
        return found + 1;
    }

    found->size -= (uint32_t)need;
    Block *const b = next(found);
    *b = (Block){.size = (uint32_t)need, .used = true};

    return b + 1;
}

static void *take(const Heap *const h, const size_t need)
{
    for (uint32_t i = 0; i < h->n; i++) {
        void *const p = i == h->grow ? nullptr : take_high(&h->region[i], need);

        if (p)
            return p;
    }

    return h->grow < h->n ? take_high(&h->region[h->grow], need) : nullptr;
}

bool heap_extend(Heap *const h, void *const p, const size_t size)
{
    const HeapRegion *const r = region_of(h, p);
    Block *const b = (Block *)p - 1;
    const uint32_t had = b->size;
    const size_t need = needed(size);
    join(r, b);

    if (b->size >= need) {
        split(b, need);
        return true;
    }

    split(b, had);
    return false;
}

void *heap_resize(Heap *const h, void *const p, const size_t size)
{
    if (!p)
        return take(h, needed(size));

    if (heap_extend(h, p, size))
        return p;

    void *const q = take(h, needed(size));

    if (q) {
        Block *const b = (Block *)p - 1;
        memcpy(q, p, b->size - sizeof(Block));
        b->used = false;
    }

    return q;
}

void heap_free(Heap *const h, void *const p)
{
    if (p)
        ((Block *)p - 1)->used = false;
}

void *heap_bottom(Heap *const h, const size_t size)
{
    const size_t need = needed(size);
    Block *best = nullptr;

    for (uint32_t i = 0; i < h->n; i++) {
        const HeapRegion *const r = &h->region[i];

        for (Block *b = (Block *)r->lo; (uint8_t *)b < r->hi; b = next(b)) {
            if (b->used)
                continue;

            join(r, b);

            if (b->size >= need && (!best || b->size > best->size)) {
                best = b;
                h->grow = i;
            }
        }
    }

    if (!best)
        return nullptr;

    split(best, need);
    best->used = true;
    return best + 1;
}

size_t heap_after(const Heap *const h, const void *const p)
{
    const HeapRegion *const r = region_of(h, p);
    const Block *const b = (const Block *)p - 1;
    size_t n = 0;

    for (const Block *f = (const Block *)((const uint8_t *)b + b->size);

         (const uint8_t *)f < r->hi && !f->used;
         f = (const Block *)((const uint8_t *)f + f->size))
        n += f->size;

    return n;
}
