/* The platform's memory on DOS32.
 *
 * # How it works
 * The flat segment from 0. The heap's regions: DOS's largest free block under 1 MB, less what
 * SHELL's program and the sound card's buffer need (DOS_KEEP), then DPMI's largest free block,
 * less what DOS/32A keeps (HOST_ROOM). The first takes the C side's blocks before the second, whose
 * bottom is the BASIC heap's.
 */
#include "dos.h"
#include "plat/pmem.h"
#include "util/heap.h"

enum {
    HOST_ROOM = 0x10000,
    PAGE_MASK = 0xFFF,
    DOS_KEEP = 0x1000,  /* paragraphs: 64K */
    DOS_LEAST = 0x100,  /* paragraphs: a block smaller is left to DOS */
    PARAGRAPH = 16,
};

static Heap heap;

static uint32_t dpmi_info[12]; /* DPMI 0500h's buffer: the largest free block first */

/* DOS's largest free block, less DOS_KEEP paragraphs, a region. */
static void dos_block(void)
{
    PdosRegs r = {.eax = DPMI_DOS_ALLOC, .ebx = 0xFFFF};
    pdos_int(INT_DPMI, &r); /* fails: ebx the most there is */
    const uint32_t most = r.ebx & 0xFFFF;

    if (most < DOS_KEEP + DOS_LEAST)
        return;

    r = (PdosRegs){.eax = DPMI_DOS_ALLOC, .ebx = most - DOS_KEEP};

    if (pdos_int(INT_DPMI, &r) & PDOS_CF)
        return;

    const uint32_t linear = (r.eax & 0xFFFF) * PARAGRAPH;
    heap_add(&heap, at(linear - pdos_ds_base()), (most - DOS_KEEP) * PARAGRAPH);
}

/* DPMI's largest free block, less HOST_ROOM, a region. */
static bool dpmi_block(void)
{
    PdosRegs r = {.eax = DPMI_MEM_INFO, .edi = (uint32_t)(uintptr_t)dpmi_info};

    pdos_int(INT_DPMI, &r);
    const uint32_t largest = dpmi_info[0];

    if (largest <= HOST_ROOM)
        return false;

    const uint32_t size = (largest - HOST_ROOM) & ~(uint32_t)PAGE_MASK;
    r = (PdosRegs){.eax = DPMI_MEM_ALLOC, .ebx = size >> 16, .ecx = size & 0xFFFF};

    if (pdos_int(INT_DPMI, &r) & PDOS_CF)
        return false;

    return heap_add(&heap, at(pdos_pair(r.ebx, r.ecx) - pdos_ds_base()), size);
}

bool pmem_init(void)
{
    heap_init(&heap);
    dos_block();
    return dpmi_block() || heap.n;
}

void *pmem_resize(void *const p, const size_t size) { return heap_resize(&heap, p, size); }

void pmem_free(void *const p) { heap_free(&heap, p); }

void *pmem_bottom(const size_t size) { return heap_bottom(&heap, size); }

bool pmem_extend(void *const p, const size_t size) { return heap_extend(&heap, p, size); }

size_t pmem_after(const void *const p) { return heap_after(&heap, p); }
