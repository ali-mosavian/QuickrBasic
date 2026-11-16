/* The platform's memory: the arena (core/base.h) as one block, and the C side's heap.
 *
 * # How it works
 * pmem_init sets mem_base and returns the free part [*lo, *hi) as Addrs: the largest block the host
 * gives, less what it keeps for itself and the C side's heap. The heap's blocks are not the arena;
 * a program does not see them.
 */
#ifndef PLAT_PMEM_H
#define PLAT_PMEM_H
#include "base.h"

/* The arena: mem_base set, and [*lo, *hi) the free part as Addrs, the largest the host gives
   less what it keeps for itself and the C side's heap, heap bytes where it has a fixed one.
   false: none, or under `least` bytes. */
bool pmem_init(uint32_t least, uint32_t heap, Addr *lo, Addr *hi);

/* p (from here, or null) as size bytes, its first bytes kept, aligned for any object: in place or
   moved. Null: none left, p kept. */
void *pmem_resize(void *p, size_t size);
void pmem_free(void *p); /* p from pmem_resize, or null */
#endif
