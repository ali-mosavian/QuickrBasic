/* The allocator's interface: one heap over several regions. How it works: heap.c. */
#ifndef UTIL_HEAP_H
#define UTIL_HEAP_H
#include <stddef.h>
#include <stdint.h>

enum { HEAP_REGIONS = 16 };

typedef struct {
    uint8_t *lo, *hi; /* its blocks, packed */
} HeapRegion;

typedef struct {
    HeapRegion region[HEAP_REGIONS]; /* in the order added: the first with room takes a block */
    uint32_t n;
    uint32_t grow; /* heap_bottom's block's region; n: none */
} Heap;

/* No regions. */
void heap_init(Heap *h);
/* All of [mem, mem + size) free, a region after the others; false: HEAP_REGIONS already. */
bool heap_add(Heap *h, void *mem, size_t size);
/* p (from h, or null) as size bytes, its first bytes kept, aligned for any object: in place, or
   moved to the top of the first region with room, heap_bottom's last; 16-aligned. Null: no room,
   p kept. */
void *heap_resize(Heap *h, void *p, size_t size);
void heap_free(Heap *h, void *p); /* p from h, or null */
/* size bytes at the bottom of the largest free room, to grow in place; null none. */
void *heap_bottom(Heap *h, size_t size);
/* p as size bytes, in place only; false: no room after it, p kept. */
bool heap_extend(Heap *h, void *p, size_t size);
/* The free bytes after p: what heap_extend can add. */
size_t heap_after(const Heap *h, const void *p);
#endif
