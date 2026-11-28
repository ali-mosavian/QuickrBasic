/* Write combining by a P6's variable-range MTRRs (mtrr.h). */
#include "plat/dos32/mtrr.h"

enum : uint32_t { PAGE = 4096, WC_TYPE = 1, VALID = 1u << 11 };

static bool valid(const MtrrPair p) { return p.mask & VALID; }

MtrrPair mtrr_combining(const uint32_t phys, const uint32_t bytes, const uint32_t width)
{
    const uint64_t top = (1ull << width) - 1;
    const bool sized = bytes >= PAGE && !(bytes & (bytes - 1)) && !(phys & (bytes - 1));

    if (!sized)
        return (MtrrPair){};

    return (MtrrPair){phys | WC_TYPE,
                      (top & ~(uint64_t)(bytes - 1) & ~(uint64_t)(PAGE - 1)) | VALID};
}

/* Whether a and b match an address alike: their bases agree under both masks. */
static bool overlap(const MtrrPair a, const MtrrPair b)
{
    const uint64_t under = a.mask & b.mask & ~(uint64_t)(PAGE - 1);

    return !((a.base ^ b.base) & under);
}

uint32_t mtrr_slot(const MtrrPair *const have, const uint32_t n, const MtrrPair want)
{
    uint32_t unused = n;

    if (!valid(want))
        return n;

    for (uint32_t i = 0; i < n; i++) {
        if (have[i].base == want.base && have[i].mask == want.mask)
            return i;

        if (!valid(have[i]))
            unused = unused < n ? unused : i;
        else if (overlap(have[i], want))
            return n;
    }

    return unused;
}
