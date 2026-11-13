/* The arena on DOS32: the flat segment from 0, and DPMI's largest free block, less what the host
   keeps (64K). */
#include "dos.h"
#include "plat/pmem.h"

enum { HOST_ROOM = 0x10000, PAGE_MASK = 0xFFF };

static uint32_t dpmi_info[12]; /* DPMI 0500h's buffer: the largest free block first */

bool pmem_init(const uint32_t least, Addr *const lo, Addr *const hi)
{
    PdosRegs r = {.eax = DPMI_MEM_INFO, .edi = (uint32_t)(uintptr_t)dpmi_info};
    pdos_int(INT_DPMI, &r);
    const uint32_t largest = dpmi_info[0];
    if (largest < HOST_ROOM)
        return false;
    const uint32_t size = (largest - HOST_ROOM) & ~(uint32_t)PAGE_MASK;
    if (size < least)
        return false;
    r = (PdosRegs){.eax = DPMI_MEM_ALLOC, .ebx = size >> 16, .ecx = size & 0xFFFF};
    if (pdos_int(INT_DPMI, &r) & PDOS_CF)
        return false;
    const uint32_t linear = pdos_pair(r.ebx, r.ecx);
    *lo = linear - pdos_ds_base();
    *hi = *lo + size;
    return true;
}
