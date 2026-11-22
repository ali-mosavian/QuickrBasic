/* The arena on DOS32.
 *
 * # How it works
 * The flat segment from 0, and DPMI's largest free block, less what the host keeps (64K) and the
 * C side's heap (util/heap.h) at its top: the front end's ask, or a quarter of the block where
 * that is more, as a native extension's memory is the C side's (gfx's canvas and pictures).
 */
#include "dos.h"
#include "plat/pmem.h"
#include "util/heap.h"

enum { HOST_ROOM = 0x10000, PAGE_MASK = 0xFFF };

static Heap c_heap;

static uint32_t dpmi_info[12]; /* DPMI 0500h's buffer: the largest free block first */

bool pmem_init(const uint32_t least, const uint32_t heap, Addr *const lo, Addr *const hi)
{
    PdosRegs r = {.eax = DPMI_MEM_INFO, .edi = (uint32_t)(uintptr_t)dpmi_info};

    pdos_int(INT_DPMI, &r);
    const uint32_t largest = dpmi_info[0];

    if (largest < HOST_ROOM)
        return false;

    const uint32_t size = (largest - HOST_ROOM) & ~(uint32_t)PAGE_MASK;
    const uint32_t quarter = size / 4 & ~(uint32_t)PAGE_MASK;
    const uint32_t c_side = heap > quarter ? heap : quarter;

    if (size < least + c_side)
        return false;

    r = (PdosRegs){.eax = DPMI_MEM_ALLOC, .ebx = size >> 16, .ecx = size & 0xFFFF};

    if (pdos_int(INT_DPMI, &r) & PDOS_CF)
        return false;

    const uint32_t linear = pdos_pair(r.ebx, r.ecx);
    *lo = linear - pdos_ds_base();
    *hi = *lo + size - c_side;
    heap_init(&c_heap);
    heap_add(&c_heap, at(*hi), c_side);
    return true;
}

void *pmem_resize(void *const p, const size_t size) { return heap_resize(&c_heap, p, size); }

void pmem_free(void *const p) { heap_free(&c_heap, p); }
