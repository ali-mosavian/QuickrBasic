/* The arena on the host.
 *
 * # How it works
 * One zeroed block. Addr 0 stays unused, as on DOS32. The C side's heap is libc's, from pmem_init
 * on.
 */
#include <stdlib.h>
#include "plat/pmem.h"

enum : uint32_t { ARENA_SIZE = 16u << 20, ARENA_LOW = 0x1000 };

uint8_t *mem_base;

bool pmem_init(const uint32_t least, const uint32_t heap, Addr *const lo, Addr *const hi)
{
    if (ARENA_SIZE - ARENA_LOW < least)
        return false;

    mem_base = calloc(1, ARENA_SIZE);

    if (!mem_base)
        return false;

    *lo = ARENA_LOW;
    *hi = ARENA_SIZE;
    return true;
}

/* Null before pmem_init, as DOS32's heap is. */
void *pmem_resize(void *const p, const size_t size)
{
    return mem_base ? realloc(p, size ? size : 1) : nullptr;
}

void pmem_free(void *const p) { free(p); }
