/* DOS memory, physical mappings and real-mode interrupts, through DPMI (dos.h). */
#include "dos.h"

bool pdos_alloc(const uint32_t size, PdosBlock *const b)
{
    PdosRegs r = {.eax = DPMI_DOS_ALLOC, .ebx = (size + 15) >> 4};

    if (pdos_int(INT_DPMI, &r) & PDOS_CF)
        return false;

    b->seg = (uint16_t)r.eax;
    b->sel = (uint16_t)r.edx;
    b->at = (uint8_t *)(uintptr_t)((uint32_t)b->seg * 16 - pdos_ds_base());
    return true;
}

void pdos_free(const PdosBlock *const b)
{
    PdosRegs r = {.eax = DPMI_DOS_FREE, .edx = b->sel};

    pdos_int(INT_DPMI, &r);
}

bool pdos_rm_int(const uint8_t vector, PdosRmRegs *const rm)
{
    PdosRegs r = {.eax = DPMI_RM_INT, .ebx = vector, .edi = (uint32_t)(uintptr_t)rm};

    return !(pdos_int(INT_DPMI, &r) & PDOS_CF);
}

volatile uint8_t *pdos_map(const uint32_t phys, const uint32_t bytes)
{
    PdosRegs r = {
        .eax = DPMI_MAP,
        .ebx = phys >> 16,
        .ecx = phys & 0xFFFF,
        .esi = bytes >> 16,
        .edi = bytes & 0xFFFF,
    };

    if (!bytes || pdos_int(INT_DPMI, &r) & PDOS_CF)
        return nullptr;

    return (volatile uint8_t *)(uintptr_t)(pdos_pair(r.ebx, r.ecx) - pdos_ds_base());
}
