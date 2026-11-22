/* The platform's memory on DOS32.
 *
 * # How it works
 * The flat segment from 0. The heap's regions: DOS's largest free block under 1 MB, less what
 * SHELL's program and the sound card's buffer need (DOS_KEEP), then every DPMI block, largest
 * first, HOST_ROOM held back for DOS/32A while they are taken. The first takes the C side's blocks
 * before the others; the largest room's bottom is the BASIC heap's.
 */
#include "dos.h"
#include "plat/pmem.h"
#include "util/heap.h"

enum {
    HOST_ROOM = 0x10000,
    PAGE_MASK = 0xFFF,
    DPMI_LEAST = 0x1000, /* a block smaller is left to DOS/32A */
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

/* A DPMI block of size bytes: its linear address, *handle DPMI's for it; 0 none. */
static uint32_t dpmi_take(const uint32_t size, uint32_t *const handle)
{
    PdosRegs r = {.eax = DPMI_MEM_ALLOC, .ebx = size >> 16, .ecx = size & 0xFFFF};

    if (pdos_int(INT_DPMI, &r) & PDOS_CF)
        return 0;

    *handle = pdos_pair(r.esi, r.edi);
    return pdos_pair(r.ebx, r.ecx);
}

static void dpmi_give(const uint32_t handle)
{
    PdosRegs r = {.eax = DPMI_MEM_FREE, .esi = handle >> 16, .edi = handle & 0xFFFF};

    pdos_int(INT_DPMI, &r);
}

/* DPMI's largest free block, whole pages. */
static uint32_t dpmi_largest(void)
{
    PdosRegs r = {.eax = DPMI_MEM_INFO, .edi = (uint32_t)(uintptr_t)dpmi_info};

    pdos_int(INT_DPMI, &r);
    return dpmi_info[0] & ~(uint32_t)PAGE_MASK;
}

/* Every DPMI block a region, the largest first, HOST_ROOM held meanwhile and given back. */
static void dpmi_blocks(void)
{
    uint32_t held = 0;

    if (!dpmi_take(HOST_ROOM, &held))
        return;

    for (uint32_t size = dpmi_largest(); size >= DPMI_LEAST; size = dpmi_largest()) {
        uint32_t handle;
        const uint32_t linear = dpmi_take(size, &handle);

        if (!linear)
            break;

        if (!heap_add(&heap, at(linear - pdos_ds_base()), size)) {
            dpmi_give(handle);
            break;
        }
    }

    dpmi_give(held);
}

bool pmem_init(void)
{
    heap_init(&heap);
    dos_block();
    dpmi_blocks();
    return heap.n;
}

void *pmem_resize(void *const p, const size_t size) { return heap_resize(&heap, p, size); }

void pmem_free(void *const p) { heap_free(&heap, p); }

void *pmem_bottom(const size_t size) { return heap_bottom(&heap, size); }

bool pmem_extend(void *const p, const size_t size) { return heap_extend(&heap, p, size); }

size_t pmem_after(const void *const p) { return heap_after(&heap, p); }
