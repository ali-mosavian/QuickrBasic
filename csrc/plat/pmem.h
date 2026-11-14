/* The platform's memory: the arena (core/base.h) as one block, and blocks for the C side's
 * allocators.
 *
 * # How it works
 * pmem_init sets mem_base and returns the free part [*lo, *hi) as Addrs: the largest block the host
 * gives, less what it keeps for itself. pmem_block's blocks are never freed and are not the arena;
 * a program does not see them.
 */
#ifndef PLAT_PMEM_H
#define PLAT_PMEM_H
#include "base.h"

/* The arena: mem_base set, and [*lo, *hi) the free part as Addrs, the largest the host gives
   less what it keeps for itself. false: none, or under `least` bytes. */
bool pmem_init(uint32_t least, Addr *lo, Addr *hi);

/* A new block of size bytes for the C side's own allocators (util/arena.h), never freed; null
   none left. Not the arena: a program does not see it. */
void *pmem_block(size_t size);
#endif
