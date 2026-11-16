/* The free-list allocator's interface. How it works: heap.c. */
#ifndef UTIL_HEAP_H
#define UTIL_HEAP_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *lo, *hi; /* its blocks, packed */
} Heap;

/* All of [mem, mem + size) free. */
void heap_init(Heap *h, void *mem, size_t size);
/* p (from h, or null) as size bytes, its first bytes kept: in place or moved, aligned for any
   object. Null: no room, p kept. */
void *heap_resize(Heap *h, void *p, size_t size);
void heap_free(Heap *h, void *p); /* p from h, or null */
#endif
