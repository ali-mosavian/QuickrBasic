/* The platform's memory: the arena (core/base.h), as one block. */
#ifndef PLAT_PMEM_H
#define PLAT_PMEM_H
#include "base.h"

/* The arena: mem_base set, and [*lo, *hi) the free part as Addrs, the largest the host gives
   less what it keeps for itself. false: none, or under `least` bytes. */
bool pmem_init(uint32_t least, Addr *lo, Addr *hi);
#endif
