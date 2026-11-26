/* The platform's memory: one heap (util/heap.h) over everything the host gives.
 *
 * # How it works
 * pmem_init sets mem_base (Addr 0) and makes every block the host gives a region of the heap:
 * the C side's buffers, the runtime's, the arena's regions and blocks, the heap arrays, and at the
 * bottom of the largest the BASIC heap, which grows up into the room after it (pmem_bottom,
 * pmem_extend). Each block is an Addr's. DOS32: DPMI's largest block, less what DOS/32A keeps, and DOS's largest
 * block under 1 MB, less what SHELL and the sound card's buffer need.
 */
#ifndef PLAT_PMEM_H
#define PLAT_PMEM_H
#include "base.h"

/* The heap made, mem_base set. false: no memory. */
bool pmem_init(void);

/* p (from here, or null) as size bytes, its first bytes kept, 16-aligned: in place or moved.
   Null: none left, p kept. */
void *pmem_resize(void *p, size_t size);
void pmem_free(void *p); /* p from here, or null */
/* size bytes at the bottom of the largest room, to grow in place: the BASIC heap's. Null none. */
void *pmem_bottom(size_t size);
/* p as size bytes, in place only; false: no room after it, p kept. */
bool pmem_extend(void *p, size_t size);
/* The free bytes after p: what pmem_extend can add. */
size_t pmem_after(const void *p);
/* The free bytes under below, every region's: what blocks can still take, about. */
size_t pmem_left(const void *below);
#endif
