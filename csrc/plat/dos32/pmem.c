/* The arena on DOS32: the flat segment from 0, and DPMI's largest free block, less what the host
   keeps (64K) and the pool pmem_block hands out from its top. */
#include "dos.h"
#include "plat/pmem.h"

enum { HOST_ROOM = 0x10000, POOL = 0x20000, PAGE_MASK = 0xFFF };

static uint8_t *pool, *pool_end; /* pmem_block's, sbrk's way */

static uint32_t dpmi_info[12]; /* DPMI 0500h's buffer: the largest free block first */

bool pmem_init(const uint32_t least, Addr *const lo, Addr *const hi)
{
    PdosRegs r = {.eax = DPMI_MEM_INFO, .edi = (uint32_t)(uintptr_t)dpmi_info};

    pdos_int(INT_DPMI, &r);
    const uint32_t largest = dpmi_info[0];

    if (largest < HOST_ROOM)
        return false;

    const uint32_t size = (largest - HOST_ROOM) & ~(uint32_t)PAGE_MASK;

    if (size < least + POOL)
        return false;

    r = (PdosRegs){.eax = DPMI_MEM_ALLOC, .ebx = size >> 16, .ecx = size & 0xFFFF};

    if (pdos_int(INT_DPMI, &r) & PDOS_CF)
        return false;

    const uint32_t linear = pdos_pair(r.ebx, r.ecx);
    *lo = linear - pdos_ds_base();
    *hi = *lo + size - POOL;
    pool = at(*hi);
    pool_end = pool + POOL;
    return true;
}

void *pmem_block(const size_t size)
{
    if ((size_t)(pool_end - pool) < size)
        return nullptr;

    void *const p = pool;
    pool += size;
    return p;
}
