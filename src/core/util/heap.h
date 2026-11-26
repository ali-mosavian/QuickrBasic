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

/* Where regions come from: the platform's memory (DOS's, DPMI's). */
typedef struct {
    size_t (*largest)(void); /* its largest free block's bytes, rounded down to a block's */
    void *(*take)(size_t size, uint32_t *handle); /* a block; null none */
    void (*give)(uint32_t handle);
    size_t keep;  /* the bytes held back while the blocks are taken */
    size_t least; /* a block smaller is left */
} HeapSource;

/* No regions. */
void heap_init(Heap *h);
/* All of [mem, mem + size) free, a region after the others; false: HEAP_REGIONS already. */
bool heap_add(Heap *h, void *mem, size_t size);
/* Every block of s at least s.least bytes a region, the largest first, s.keep bytes held back
   meanwhile and given back after. */
void heap_take_all(Heap *h, const HeapSource *s);
/* p (from h, or null) as size bytes, its first bytes kept, aligned for any object: in place, or
   moved to the top of the first region with room, heap_bottom's last; 16-aligned. Null: no room,
   p kept. */
void *heap_resize(Heap *h, void *p, size_t size);
void heap_free(Heap *h, void *p); /* p from h, or null */
/* size bytes at the bottom of the largest free room, to grow in place; null none. */
void *heap_bottom(Heap *h, size_t size);
/* p as size bytes, in place only; false: no room after it, p kept. */
bool heap_extend(Heap *h, void *p, size_t size);
/* The free blocks' bytes under below in every region, their headers too: the same however they
   lie, what blocks can still take about. */
size_t heap_left(const Heap *h, const void *below);
/* The free bytes after p: what heap_extend can add. */
size_t heap_after(const Heap *h, const void *p);
#endif
