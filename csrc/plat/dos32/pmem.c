/* The arena on DOS32: the flat segment from 0, and DPMI's largest free block, less what the host
   keeps (64K). */
#include "dos.h"
#include "plat/pmem.h"

enum { HOST_ROOM = 0x10000 };


static uint32_t dpmi_info[12]; /* DPMI 0500h's buffer */

bool pmem_init(uint32_t least, Addr *lo, Addr *hi)
{
    PdosRegs r = {.eax = 0x0500, .edi = (uint32_t)(uintptr_t)dpmi_info};
    pdos_int(0x31, &r);
    uint32_t size = dpmi_info[0];
    if (size < HOST_ROOM)
        return false;
    size = (size - HOST_ROOM) & ~0xFFFu;
    if (size < least)
        return false;
    r = (PdosRegs){.eax = 0x0501, .ebx = size >> 16, .ecx = size & 0xFFFF};
    if (pdos_int(0x31, &r) & PDOS_CF)
        return false;
    uint32_t linear = (r.ebx << 16) | (r.ecx & 0xFFFF);
    r = (PdosRegs){.eax = 0x0006, .ebx = pdos_ds()};
    pdos_int(0x31, &r);
    uint32_t ds_base = (r.ecx << 16) | (r.edx & 0xFFFF);
    *lo = linear - ds_base;
    *hi = *lo + size;
    return true;
}
