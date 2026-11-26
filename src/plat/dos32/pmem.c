/* The platform's memory on DOS32.
 *
 * # How it works
 * The flat segment from 0. The heap's regions: every free DOS block under 1 MB, then every DPMI
 * block, each source's largest first. While a source's blocks are taken, a block of its own is
 * held back and then given back: DOS_KEEP for SHELL's program and the sound card's buffer,
 * HOST_ROOM for DOS/32A. The DOS blocks take the C side's blocks before the others; the largest
 * room's bottom is the BASIC heap's.
 */
#include "dos.h"
#include "plat/pmem.h"
#include "util/heap.h"

enum : uint32_t {
    PAGE_MASK = 0xFFF,
    PARAGRAPH = 16,
    HOST_ROOM = 0x10000,
    DPMI_LEAST = 0x1000, /* a block smaller is left to DOS/32A */
    DOS_KEEP = 0x10000,
    DOS_LEAST = 0x1000, /* a block smaller is left to DOS */
};

static Heap heap;

static uint32_t dpmi_info[12]; /* DPMI 0500h's buffer: the largest free block first */

static size_t dos_largest(void)
{
    PdosRegs r = {.eax = DPMI_DOS_ALLOC, .ebx = 0xFFFF};

    pdos_int(INT_DPMI, &r); /* fails: ebx the most there is */
    return (r.ebx & 0xFFFF) * PARAGRAPH;
}

static void *dos_take(const size_t size, uint32_t *const handle)
{
    PdosRegs r = {.eax = DPMI_DOS_ALLOC, .ebx = (uint32_t)size / PARAGRAPH};

    if (pdos_int(INT_DPMI, &r) & PDOS_CF)
        return nullptr;

    *handle = r.edx & 0xFFFF; /* its selector */
    return at((r.eax & 0xFFFF) * PARAGRAPH - pdos_ds_base());
}

static void dos_give(const uint32_t handle)
{
    PdosRegs r = {.eax = DPMI_DOS_FREE, .edx = handle};

    pdos_int(INT_DPMI, &r);
}

static size_t dpmi_largest(void)
{
    PdosRegs r = {.eax = DPMI_MEM_INFO, .edi = (uint32_t)(uintptr_t)dpmi_info};

    pdos_int(INT_DPMI, &r);
    return dpmi_info[0] & ~PAGE_MASK;
}

static void *dpmi_take(const size_t size, uint32_t *const handle)
{
    PdosRegs r = {.eax = DPMI_MEM_ALLOC, .ebx = (uint32_t)size >> 16, .ecx = size & 0xFFFF};

    if (pdos_int(INT_DPMI, &r) & PDOS_CF)
        return nullptr;

    *handle = pdos_pair(r.esi, r.edi);
    return at(pdos_pair(r.ebx, r.ecx) - pdos_ds_base());
}

static void dpmi_give(const uint32_t handle)
{
    PdosRegs r = {.eax = DPMI_MEM_FREE, .esi = handle >> 16, .edi = handle & 0xFFFF};

    pdos_int(INT_DPMI, &r);
}

bool pmem_init(void)
{
    static const HeapSource dos = {dos_largest, dos_take, dos_give, DOS_KEEP, DOS_LEAST};

    static const HeapSource dpmi = {dpmi_largest, dpmi_take, dpmi_give, HOST_ROOM, DPMI_LEAST};

    heap_init(&heap);
    heap_take_all(&heap, &dos);
    heap_take_all(&heap, &dpmi);
    return heap.n;
}

void *pmem_resize(void *const p, const size_t size) { return heap_resize(&heap, p, size); }

void pmem_free(void *const p) { heap_free(&heap, p); }

void *pmem_bottom(const size_t size) { return heap_bottom(&heap, size); }

bool pmem_extend(void *const p, const size_t size) { return heap_extend(&heap, p, size); }

size_t pmem_after(const void *const p) { return heap_after(&heap, p); }
