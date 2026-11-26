/* VBE's controller and its modes, through DPMI (dos.h). */
#include "dos.h"
#include "plat/pmem.h"
#include <string.h>

enum : uint16_t {
    VBE_INFO = 0x4F00,
    VBE_MODE_INFO = 0x4F01,
    VBE_SET = 0x4F02,
    VBE_OK = 0x004F,
    VBE_END = 0xFFFF,
    VBE_USE_LINEAR = 0x4000,
};
enum : uint32_t {
    MEMORY_STEP = 0x10000,
    MODES_MOST = 128,
    BDA_CURSORS = 0x450, /* the cursors, their shape and the page shown */
    BDA_CURSOR_BYTES = 0x463 - BDA_CURSORS,
};

static struct {
    bool listed;
    VbeMode *modes;
    uint32_t n;
    PdosVbe card;
} vbe;

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

/* The modes gfx shows in. */
static void listed(void)
{
    vbe.listed = true;
    vbe.modes = pmem_resize(nullptr, (MODES_MOST + 1) * sizeof *vbe.modes);

    if (!vbe.modes)
        return;

    vbe.modes[vbe.n++] = vbe_mode_13h();
    PdosBlock b;

    if (!pdos_alloc(PDOS_VBE_BLOCK, &b))
        return;

    uint16_t numbers[MODES_MOST];
    const uint32_t n = pdos_vbe_modes(&b, &vbe.card, numbers, MODES_MOST);

    for (uint32_t i = 0; i < n; i++)
        if (pdos_vbe_mode(&b, numbers[i]) &&
            vbe_mode(numbers[i], vbe.card.version, b.at, &vbe.modes[vbe.n]))
            vbe.n++;

    pdos_free(&b);
}

const VbeMode *pdos_vbe_list(uint32_t *const n, PdosVbe *const card)
{
    if (!vbe.listed)
        listed();

    *n = vbe.n;
    *card = vbe.card;
    return vbe.modes;
}

bool pdos_vbe_set(const VbeMode *const m)
{
    uint8_t cursors[BDA_CURSOR_BYTES];

    /* a mode set zeroes them */
    memcpy(cursors, (const uint8_t *)pdos_lin(BDA_CURSORS), sizeof cursors);

    if (m->number == VBE_MODE_13H) {
        PdosRegs r = {.eax = VBE_MODE_13H};

        pdos_int(INT_VIDEO, &r);
    } else {
        PdosRmRegs rm = {.eax = VBE_SET, .ebx = m->number | (m->linear ? VBE_USE_LINEAR : 0)};

        if (!pdos_rm_int(INT_VIDEO, &rm) || (uint16_t)rm.eax != VBE_OK)
            return false;
    }

    memcpy((uint8_t *)pdos_lin(BDA_CURSORS), cursors, sizeof cursors);
    return true;
}
