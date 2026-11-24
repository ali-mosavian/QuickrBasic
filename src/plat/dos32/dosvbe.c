/* VBE's controller and its modes, through DPMI (dos.h). */
#include "dos.h"
#include <string.h>

enum : uint16_t { VBE_INFO = 0x4F00, VBE_MODE_INFO = 0x4F01, VBE_OK = 0x004F, VBE_END = 0xFFFF };
enum : uint32_t { MEMORY_STEP = 0x10000 };

/* VBE's controller information (4F00h): what is read of it. */
typedef struct __attribute__((packed)) {
    char sig[4];
    uint16_t version;
    uint32_t oem;
    uint32_t caps;
    uint16_t modes_off, modes_seg;
    uint16_t memory; /* 64 KB steps */
} VbeInfo;
static_assert(offsetof(VbeInfo, memory) == 0x12);

uint32_t pdos_vbe_modes(
    const PdosBlock *const b,
    PdosVbe *const card,
    uint16_t *const modes,
    const uint32_t most
)
{
    memcpy(b->at, "VBE2", 4);
    PdosRmRegs rm = {.eax = VBE_INFO, .es = b->seg};

    if (!pdos_rm_int(INT_VIDEO, &rm) || (uint16_t)rm.eax != VBE_OK || memcmp(b->at, "VESA", 4))
        return 0;

    /* the list may lie in the block, which the next call reuses */
    const VbeInfo *const info = (const VbeInfo *)b->at;
    const volatile uint16_t *const ids =
        (const volatile uint16_t *)pdos_lin(info->modes_seg * 16u + info->modes_off);
    uint32_t n = 0;

    for (; n < most && ids[n] != VBE_END; n++)
        modes[n] = ids[n];

    *card = (PdosVbe){info->version, info->memory * MEMORY_STEP};

    return n;
}

bool pdos_vbe_mode(const PdosBlock *const b, const uint16_t mode)
{
    PdosRmRegs rm = {.eax = VBE_MODE_INFO, .ecx = mode, .es = b->seg};

    return pdos_rm_int(INT_VIDEO, &rm) && (uint16_t)rm.eax == VBE_OK;
}
