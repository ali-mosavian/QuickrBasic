/* An SST card's RAMDAC (dac.h), as Glide drives it (glide3x/sst1/init/initvg/dac.c). */
#include "plat/dos32/drivers/3dfx/dac.h"
#include "plat/dos32/drivers/3dfx/regs.h"

enum : uint32_t {
    INIT_ENABLE = 0x40, /* PCI's initEnable: init writes, PCI FIFO writes, fbiInit2 reads the DAC */
    INIT_WRITES = 1,
    FIFO_WRITES = 2,
    DAC_READS = 4,
    VIDEO_RESET = 1u << 8, /* fbiInit1's */
    REFRESH = 1u << 22,    /* fbiInit2's */
    GRAPHICS_RESET = 1u << 1, /* fbiInit0's */
    FIFO_RESET = 1u << 2,
    DAC_REG = 8, /* dacData: data 7:0, register 10:8, a read */
    DAC_READ = 1u << 11,
    TRIES = 100,
    SETTLE_US = 100000, /* a PLL's: Glide reads status 200000 times */
    FIRST_TEXELS = 0xF00000, /* written once the graphics clock is set: the first download's fix */
};

/* The DACs' registers: the read mask the AT&T's backdoor, its index and data once indexed;
   the ICS's PLL write index, data, command and read index. */
enum : uint32_t { WRITE_ADDRESS = 0, MASK = 2, ICS_PLL_WRITE = 4, ICS_PLL_DATA = 5, ICS_COMMAND = 6, ICS_PLL_READ = 7 };

/* The ICS's PLLs: pixel clock 0, graphics clock 0, control; GCLK1's, VCLK1's and VCLK7's power-on
   words; its control's clock selects; its 16-bit mode. */
enum : uint8_t {
    VCLK0 = 0x0,
    GCLK0 = 0xA,
    PLL_CONTROL = 0xE,
    GCLK1 = 0xB,
    GCLK1_ON = 0x79,
    VCLK1 = 0x1,
    VCLK1_ON = 0x55,
    VCLK7 = 0x7,
    VCLK7_ON = 0x71,
    CLK0_SELECT = 0x20,
    CLK1_SELECT = 0x10,
    CLK0_FREQUENCY = 0x7,
    ICS_16BPP = 0x50,
};

/* The AT&T's and TI's indices: CR0, its maker and device, the clock control; the pixel and
   graphics PLLs' first; their makers' and device's IDs; CR0's indexing, 8-bit DAC and 16-bit
   mode; the clock control's selects of PLL D and B. */
enum : uint8_t {
    CR0 = 0x1,
    MAKER = 0x2,
    DEVICE = 0x3,
    CLOCKS = 0x6,
    AD0 = 0x4C,
    BD0 = 0x6C,
    ATT = 0x84,
    TI = 0x97,
    ATT_DEVICE = 0x9,
    INDEXED = 0x1,
    EIGHT_BIT = 0x2,
    ATT_16BPP = 0x30,
    MODE_BITS = 0xF0,
    SELECT_AD = 0xB0,
    SELECT_BD = 0x0B,
};

static const float REFERENCE = 14.318f;

bool tdfx_clock(const float mhz, TdfxClock *const out)
{
    uint32_t p = 4;

    for (uint32_t k = 0; k < 4; k++)
        p = mhz * (float)(1u << k) >= 120 && mhz * (float)(1u << k) <= 240 ? k : p;

    if (p > 3)
        return false;

    const float ratio = mhz * (float)(1u << p) / REFERENCE;

    float least = 9999;
    uint32_t n = 0, m = 0;

    for (uint32_t n2 = 3; n2 < 32; n2++) {
        const int32_t m2 = (int32_t)((float)n2 * ratio + 0.5f);
        const float e = (float)m2 / (float)n2 - ratio, error = e < 0 ? -e : e;

        if (error < least && m2 - 2 < 127) {
            least = error;
            n = n2 - 2;
            m = (uint32_t)m2 - 2;
        }
    }

    if (!n)
        return false;

    const uint8_t loop = mhz < 37 ? 0xA6 : mhz < 45 ? 0xC4 : mhz < 58 ? 0x84 : mhz < 66 ? 0xA6 : 0xA8;

    *out = (TdfxClock){(uint8_t)m, (uint8_t)n, (uint8_t)p, loop};
    return true;
}

float tdfx_clock_mhz(const TdfxClock *const c)
{
    return REFERENCE * (float)(c->m + 2) / ((float)(c->n + 2) * (float)(1u << c->p));
}

static void init_enabled(Tdfx *const c, const uint32_t v)
{
    c->host->pci_write(&c->pci, INIT_ENABLE, v);
    tdfx_idle_fbi(c);
}

static void dac_put(Tdfx *const c, const uint32_t reg, const uint32_t v)
{
    tdfx_put_init(c, SST_DAC, (v & 0xFF) | reg << DAC_REG);
    tdfx_idle_fbi(c);
}

static uint32_t dac_get(Tdfx *const c, const uint32_t reg)
{
    tdfx_put_init(c, SST_DAC, reg << DAC_REG | DAC_READ);
    tdfx_idle_fbi(c);
    return tdfx_get(c, SST_FBI_INIT2) & 0xFF;
}

/* The DAC's alone on the memory bus; the init registers saved to *keep. */
static void began(Tdfx *const c, uint32_t keep[2])
{
    tdfx_idle_fbi(c);
    keep[0] = tdfx_get(c, SST_FBI_INIT1);
    keep[1] = tdfx_get(c, SST_FBI_INIT2);
    tdfx_put_init(c, SST_FBI_INIT1, keep[0] | VIDEO_RESET);
    tdfx_put_init(c, SST_FBI_INIT2, keep[1] & ~REFRESH);
    init_enabled(c, INIT_WRITES | DAC_READS);
}

static void ended(Tdfx *const c, const uint32_t keep[2])
{
    init_enabled(c, INIT_WRITES | FIFO_WRITES);
    tdfx_put_init(c, SST_FBI_INIT1, keep[0]);
    tdfx_put_init(c, SST_FBI_INIT2, keep[1]);
    tdfx_idle_fbi(c);
}

/* An ICS PLL's power-on word. */
static uint32_t ics_word(Tdfx *const c, const uint8_t pll)
{
    dac_put(c, ICS_PLL_READ, pll);
    const uint32_t first = dac_get(c, ICS_PLL_DATA);
    dac_get(c, ICS_PLL_DATA);
    return first;
}

static void indexed_put(Tdfx *const c, const uint8_t index, const uint32_t v)
{
    dac_put(c, WRITE_ADDRESS, index);
    dac_put(c, MASK, v);
}

static uint32_t indexed_get(Tdfx *const c, const uint8_t index)
{
    dac_put(c, WRITE_ADDRESS, index);
    return dac_get(c, MASK);
}

/* The AT&T's or TI's indexing on: false, it does not answer as one. */
static bool indexing(Tdfx *const c)
{
    for (uint32_t t = 0; t < TRIES; t++) {
        dac_put(c, WRITE_ADDRESS, 0);

        for (uint32_t i = 0; i < 4; i++)
            dac_get(c, MASK);

        const uint32_t cr0 = dac_get(c, MASK);

        dac_put(c, WRITE_ADDRESS, 0);

        for (uint32_t i = 0; i < 4; i++)
            dac_get(c, MASK);

        dac_put(c, MASK, (cr0 & MODE_BITS) | INDEXED | EIGHT_BIT);

        const uint32_t maker = indexed_get(c, MAKER);

        if ((maker == ATT || maker == TI) && indexed_get(c, DEVICE) == ATT_DEVICE)
            return true;
    }

    return false;
}

static void unindexed(Tdfx *const c) { indexed_put(c, CR0, indexed_get(c, CR0) & ~(uint32_t)INDEXED); }

TdfxDac tdfx_dac(Tdfx *const c)
{
    uint32_t keep[2];
    TdfxDac found = TDFX_DAC_NONE;

    began(c, keep);

    for (uint32_t t = 0; t < TRIES && !found; t++)
        found = ics_word(c, GCLK1) == GCLK1_ON && ics_word(c, VCLK1) == VCLK1_ON &&
                        ics_word(c, VCLK7) == VCLK7_ON
                    ? TDFX_DAC_ICS
                    : TDFX_DAC_NONE;

    if (!found && indexing(c)) {
        found = TDFX_DAC_ATT;
        unindexed(c);
    }

    ended(c, keep);
    return found;
}

/* An ICS PLL set: its M, then P and N, its clock selected in the control. */
static void ics_pll(Tdfx *const c, const uint8_t pll, const TdfxClock *const k, const bool video)
{
    dac_put(c, ICS_PLL_READ, PLL_CONTROL);

    const uint32_t control = dac_get(c, ICS_PLL_DATA);

    dac_put(c, ICS_PLL_WRITE, pll);
    dac_put(c, ICS_PLL_DATA, k->m);
    dac_put(c, ICS_PLL_DATA, (uint32_t)k->p << 5 | k->n);
    dac_put(c, ICS_PLL_WRITE, PLL_CONTROL);
    dac_put(c, ICS_PLL_DATA, video ? (control & ~(uint32_t)CLK0_FREQUENCY) | CLK0_SELECT
                                   : control & ~(uint32_t)CLK1_SELECT);
}

/* An AT&T or TI PLL set from its first index, selected in the clock control. */
static void att_pll(Tdfx *const c, const uint8_t first, const TdfxClock *const k, const uint8_t select)
{
    indexed_put(c, first, k->m);
    indexed_put(c, first + 1, (uint32_t)k->p << 6 | k->n);
    indexed_put(c, first + 2, k->loop);
    indexed_put(c, CLOCKS, indexed_get(c, CLOCKS) | select);
}

/* Status read until a PLL has settled. */
static void settled(Tdfx *const c)
{
    const uint64_t until = c->host->clock_us() + SETTLE_US;

    while (c->host->clock_us() < until)
        tdfx_get(c, SST_STATUS);
}

void tdfx_dac_video(Tdfx *const c, const TdfxDac dac, const TdfxClock *const clock)
{
    uint32_t keep[2];

    began(c, keep);

    if (dac == TDFX_DAC_ICS) {
        ics_pll(c, VCLK0, clock, true);
        dac_put(c, ICS_COMMAND, ICS_16BPP);
    } else if (dac == TDFX_DAC_ATT && indexing(c)) {
        att_pll(c, AD0, clock, SELECT_AD);
        indexed_put(c, CR0, (indexed_get(c, CR0) & ~(uint32_t)MODE_BITS) | ATT_16BPP | EIGHT_BIT);
        unindexed(c);
    }

    ended(c, keep);
    settled(c);
}

void tdfx_dac_graphics(Tdfx *const c, const TdfxDac dac, const TdfxClock *const clock)
{
    const uint32_t init0 = tdfx_get(c, SST_FBI_INIT0);

    uint32_t keep[2];

    tdfx_put_init(c, SST_FBI_INIT0, init0 | GRAPHICS_RESET | FIFO_RESET);
    tdfx_idle_fbi(c);
    began(c, keep);

    if (dac == TDFX_DAC_ICS) {
        ics_pll(c, GCLK0, clock, false);
    } else if (dac == TDFX_DAC_ATT && indexing(c)) {
        att_pll(c, BD0, clock, SELECT_BD);
        unindexed(c);
    }

    ended(c, keep);
    settled(c);
    tdfx_put_init(c, SST_FBI_INIT0, (init0 | GRAPHICS_RESET) & ~FIFO_RESET);
    tdfx_idle_fbi(c);
    tdfx_put_init(c, SST_FBI_INIT0, init0 & ~(GRAPHICS_RESET | FIFO_RESET));
    tdfx_idle_fbi(c);
    *(volatile uint32_t *)(c->regs + FIRST_TEXELS) = 0xDEADBEEF;
}
