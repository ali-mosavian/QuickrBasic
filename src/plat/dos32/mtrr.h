/* Write combining by a P6's variable-range MTRRs, worked out: pure, no CPU (dosmem.c writes them).
 *
 * # How it works
 * A range is a pair of MSRs: PHYSBASE, its base and memory type, and PHYSMASK, the address bits
 * it matches on, valid at bit 11. An address is in it when its bits under the mask are the
 * base's. A new range is put only where no valid one overlaps it, as the type two overlapping
 * ranges give is undefined unless one is UC; one that already makes it write-combined is kept.
 */
#ifndef PLAT_DOS32_MTRR_H
#define PLAT_DOS32_MTRR_H
#include <stdint.h>

/* MTRRcap's: the variable ranges, write combining a type; the most kept. */
enum : uint32_t { MTRR_RANGES = 0xFF, MTRR_WC = 1u << 10, MTRR_MOST = 16 };
/* MTRRdefType's: MTRRs on. */
enum : uint32_t { MTRR_ON = 1u << 11 };

/* A variable range's PHYSBASE and PHYSMASK. */
typedef struct {
    uint64_t base, mask;
} MtrrPair;

/* The pair making phys up to bytes write-combined, the CPU's physical addresses width bits: bytes
   a power of two of 4 KB up, phys aligned to it; else a pair not valid. */
MtrrPair mtrr_combining(uint32_t phys, uint32_t bytes, uint32_t width);
/* Where in have's n pairs want goes: one already the same, or the first unused; n, none, as one
   valid overlaps it or none is unused. */
uint32_t mtrr_slot(const MtrrPair *have, uint32_t n, MtrrPair want);
#endif
