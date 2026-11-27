/* plat/dos32/mtrr.c: write-combining ranges worked out against pairs derived by hand. */
#include "plat/dos32/mtrr.h"
#include "unit.h"

enum : uint64_t {
    VALID = 1u << 11,
    TOP = 0xFull << 32, /* a P6's 36 address bits above 32 */
    RAM = 0x00000006,   /* the BIOS's: 128 MB of RAM write-back */
    RAM_MASK = TOP | 0xF8000000u | VALID,
};

/* 16 MB at 0xE0000000, 36 address bits: the base its address and WC's type 1, the mask its 12
   bits of 16 MB up to bit 35, valid. */
TEST(mtrr_combining_masks_the_window_by_its_size)
{
    const MtrrPair p = mtrr_combining(0xE0000000, 16 << 20, 36);

    CHECK(p.base == 0xE0000001 && p.mask == (TOP | 0xFF000000u | VALID));
    CHECK(mtrr_combining(0xE0000000, 16 << 20, 32).mask == (0xFF000000u | VALID));
}

/* A size not a power of two, a base not aligned to it, or under a page: no range. */
TEST(mtrr_combining_refuses_what_a_range_cannot_be)
{
    CHECK(!(mtrr_combining(0xE0000000, 24 << 20, 36).mask & VALID));
    CHECK(!(mtrr_combining(0xE0800000, 16 << 20, 36).mask & VALID));
    CHECK(!(mtrr_combining(0xE0000000, 2048, 36).mask & VALID));
}

/* Past the BIOS's RAM, the first unused; the same pair, where it is; a UC range over the PCI
   hole overlapping, or none unused, none: two types over one window are undefined. */
TEST(mtrr_slot_takes_an_unused_range_none_overlaps)
{
    const MtrrPair want = mtrr_combining(0xE0000000, 16 << 20, 36);
    const MtrrPair ram = {RAM, RAM_MASK}, hole = {0xE0000000, TOP | 0xE0000000u | VALID};
    const MtrrPair free[3] = {ram, {}, {}}, same[3] = {ram, {}, want};
    const MtrrPair over[3] = {ram, hole, {}}, full[2] = {ram, ram};

    CHECK(mtrr_slot(free, 3, want) == 1);
    CHECK(mtrr_slot(same, 3, want) == 2);
    CHECK(mtrr_slot(over, 3, want) == 3);
    CHECK(mtrr_slot(full, 2, want) == 2);
}
