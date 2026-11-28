/* DOS memory, physical mappings and real-mode interrupts, through DPMI; write combining, by the
   MTRRs (dos.h). */
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

enum : uint32_t {
    MTRR_FEATURE = 1u << 12, /* CPUID 1's EDX */
    PAE = 1u << 6,
    CAP = 0xFE, /* MSRs: MTRRcap, MTRRdefType, PHYSBASE0 */
    DEF_TYPE = 0x2FF,
    PHYS_BASE = 0x200,
    EXTENDED = 0x80000000, /* CPUID's: its extended leaves, the address widths' */
    WIDTHS = 0x80000008,
    WIDTH = 0xFF,
};

/* The range combine changed, and what it held: none, held false. */
static struct {
    bool held;
    uint32_t at;
    MtrrPair was;
} changed;

/* The CPU's physical address bits: CPUID's, else 36 with PAE, else 32. */
static uint32_t width_of(const uint32_t features)
{
    uint32_t r[4];

    if (pdos_cpuid(EXTENDED, r) && r[0] >> 16 == EXTENDED >> 16 && r[0] >= WIDTHS &&
        pdos_cpuid(WIDTHS, r))
        return r[0] & WIDTH;

    return features & PAE ? 36 : 32;
}

static MtrrPair read_pair(const uint32_t i)
{
    return (MtrrPair){pdos_rdmsr(PHYS_BASE + 2 * i), pdos_rdmsr(PHYS_BASE + 2 * i + 1)};
}

bool pdos_combine(const uint32_t phys, const uint32_t bytes)
{
    uint32_t id[4];

    if (changed.held || pdos_cs() & 3 || !pdos_cpuid(1, id) || !(id[3] & MTRR_FEATURE))
        return false;

    const uint64_t cap = pdos_rdmsr(CAP);
    const uint32_t ranges = (uint32_t)cap & MTRR_RANGES;
    const uint32_t n = ranges < MTRR_MOST ? ranges : MTRR_MOST;

    if (!(cap & MTRR_WC) || !(pdos_rdmsr(DEF_TYPE) & MTRR_ON))
        return false;

    MtrrPair have[MTRR_MOST];

    for (uint32_t i = 0; i < n; i++)
        have[i] = read_pair(i);

    const MtrrPair want = mtrr_combining(phys, bytes, width_of(id[3]));
    const uint32_t at = mtrr_slot(have, n, want);

    if (at == n)
        return false;

    if (have[at].base == want.base && have[at].mask == want.mask)
        return true;

    pdos_mtrr_write(at, &want);

    const MtrrPair got = read_pair(at);

    if (got.base != want.base || got.mask != want.mask) {
        pdos_mtrr_write(at, &have[at]);
        return false;
    }

    changed.held = true;
    changed.at = at;
    changed.was = have[at];
    return true;
}

void pdos_uncombine(void)
{
    if (changed.held)
        pdos_mtrr_write(changed.at, &changed.was);

    changed.held = false;
}
