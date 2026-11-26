/* VOODOO1.DRV: the Voodoo Graphics (SST-1), brought up and shown as Glide does
 * (glide3x/sst1/init/initvg: sst1InitRegisters, sst1InitVideo, sst1InitShutdown).
 *
 * # How it works
 * Its modes are Glide's: 512x384, 640x400 and 640x480, the screen centred in the smallest holding
 * it. Its memory is taken as 2 MB of frame buffer and 2 MB of textures, every board's least: two
 * colour buffers and the depth buffer, a third each, the rows past the mode a target's. The memory
 * FIFO is off: writes wait for the PCI FIFO's room. A board whose fbiInit3 bit 12 is set at power-on
 * shows the VGA with fbiInit0 bit 0 set, not clear.
 */
#include "plat/dos32/drivers/3dfx/dac.h"
#include "plat/dos32/drivers/3dfx/regs.h"
#include "plat/dos32/drivers/3dfx/tdfx.h"
#include "qx/qx.h"

/* PCI's: the command register and its memory decode, the revision, BAR0, initEnable, the snoops,
   the video clock on and off. */
enum : uint8_t {
    COMMAND = 0x04,
    REVISION = 0x08,
    BAR0 = 0x10,
    INIT_ENABLE = 0x40,
    SNOOP0 = 0x44,
    SNOOP1 = 0x48,
    VIDEO_ON = 0xC0,
    VIDEO_OFF = 0xE0,
};

enum : uint32_t {
    MEMORY = 2,
    BAR_MASK = 0xFF000000,
    CARD_BYTES = 16 << 20,
    INIT_WRITES = 1,
    FIFO_WRITES = 2,
    MB2 = 2 << 20,
    PAGE = 4096,
    TILE = 64, /* pixels a tile across, two of fbiInit1's */
    ROWS_MOST = 1024, /* the clip's and the LFB's 10 bits */

    /* fbiInit0: the Voodoo shown, the graphics and PCI FIFO reset, its low-water mark, the
       memory FIFO */
    SHOWN = 1u << 0,
    GRAPHICS_RESET = 1u << 1,
    FIFO_RESET = 1u << 2,
    PCI_LWM = 6,
    PCI_LWM_BITS = 0x1Fu << 6,
    PCI_LWM_AT = 13,
    MEMORY_FIFO = 1u << 13,
    /* fbiInit1: write wait states, LFB reads, video reset, blank, tiles' shift, the outputs on,
       16-bit video, the clock delays' shifts, and the bits they rewrite */
    WAIT_STATES = 1u << 1,
    LFB_READS = 1u << 3,
    VIDEO_RESET = 1u << 8,
    BLANK = 1u << 12,
    TILES = 4,
    TILES_BITS = 0xFu << 4,
    OUTPUTS = 0xFu << 13,
    VIDEO_16BPP = 3u << 20,
    OUT_DELAY = 27,
    VIDEO_BITS = 0x7E7FFE00,
    /* fbiInit2: fast memory, swaps at vsync, pages' shift, refresh on and its rate */
    FAST_MEMORY = 1u << 5 | 1u << 7 | 1u << 21,
    SWAP_BITS = 3u << 9,
    PAGES = 11,
    PAGES_BITS = 0x1FFu << 11,
    REFRESH = 1u << 22,
    REFRESH_RATE = 0x30u << 23,
    REFRESH_BITS = 0x1FFu << 23,
    /* fbiInit3: polarity read at power-on, the video FIFO threshold's shift, the FBI-to-TMU
       delay's */
    POLARITY = 1u << 12,
    VIDEO_FIFO = 1,
    VIDEO_FIFO_BITS = 0x1Fu << 1,
    FT_DELAY = 13,
    FT_BITS = 0xFu << 13,
    /* fbiInit4: LFB read-ahead */
    READ_AHEAD = 1u << 1,

    /* the init registers' values as reset; trexInit0 and 1's */
    INIT0 = 0x410,
    INIT1 = 0x00201102,
    INIT2 = 0x80000040,
    INIT4 = 0x1,
    TREX_INIT0 = 0x31C,
    TREX_INIT1 = 0x320,
    TREX0 = 0x5441,
    TREX1 = 0xF420,
    TMUS = 3,

    STALLS = 3,
};

static const float GRAPHICS_MHZ = 50, BRING_UP_MHZ = 16, SHUTDOWN_MHZ = 30;

/* Glide's SST-1 modes, 16 bits a pixel out: their timing's words, tiles across over 2, video FIFO
   threshold and pixel clock. */
typedef struct {
    uint16_t w, h;
    uint32_t hsync, vsync, back_porch;
    uint8_t tiles, video_fifo;
    float mhz;
} Mode;

static const Mode MODES[] = {
    {512, 384, 0x024F0033, 0x01AE0003, 0x00190037, 8, 23, 20.093f},
    {640, 400, 0x02C00060, 0x01BF0002, 0x00230030, 10, 21, 25.175f},
    {640, 480, 0x02C00060, 0x020B0002, 0x00190026, 10, 23, 25.175f},
};

static struct {
    TdfxDac dac;
    uint32_t shows; /* fbiInit0's bit 0 with the Voodoo shown */
} sst;

/* The smallest mode holding w by h: null, none. */
static const Mode *mode_for(const uint16_t w, const uint16_t h)
{
    const Mode *best = nullptr;

    for (uint32_t i = 0; i < sizeof MODES / sizeof *MODES; i++) {
        const Mode *const m = &MODES[i];

        if (m->w >= w && m->h >= h && (!best || (uint32_t)m->w * m->h < (uint32_t)best->w * best->h))
            best = m;
    }

    return best;
}

static void changed(Tdfx *const c, const uint32_t reg, const uint32_t clear, const uint32_t set)
{
    tdfx_put_init(c, reg, (tdfx_get(c, reg) & ~clear) | set);
    tdfx_idle_fbi(c);
}

static void stalled(const Tdfx *const c)
{
    for (uint32_t i = 0; i < STALLS; i++)
        tdfx_get(c, SST_STATUS);
}

/* The graphics unit and PCI FIFO reset, then let run. */
static void reset(Tdfx *const c)
{
    changed(c, SST_FBI_INIT0, 0, GRAPHICS_RESET | FIFO_RESET);
    changed(c, SST_FBI_INIT0, FIFO_RESET, 0);
    changed(c, SST_FBI_INIT0, GRAPHICS_RESET, 0);
}

/* sst1InitRegisters: the chips reset to Glide's values, the DAC found, the DRAM refreshed. */
static bool registered(Tdfx *const c, const uint32_t revision)
{
    const AccelHost *const host = c->host;
    const uint32_t ft = revision >= 2 ? 0x8 : 0xA;

    TdfxClock bring_up;

    host->pci_write(&c->pci, VIDEO_OFF, 0);
    host->pci_write(&c->pci, INIT_ENABLE, INIT_WRITES);
    host->pci_write(&c->pci, SNOOP0, 0);
    host->pci_write(&c->pci, SNOOP1, 0);
    stalled(c);
    tdfx_put_init(c, SST_FBI_INIT3, ft << FT_DELAY | 8u << 17);
    tdfx_put_init(c, SST_FBI_INIT1, tdfx_get(c, SST_FBI_INIT1) | VIDEO_RESET);
    stalled(c);
    reset(c);
    tdfx_put_init(c, SST_FBI_INIT0, INIT0);
    tdfx_put_init(c, SST_FBI_INIT1, INIT1);
    tdfx_put_init(c, SST_FBI_INIT2, INIT2);
    tdfx_put_init(c, SST_FBI_INIT3, ft << FT_DELAY | 8u << 17);
    tdfx_put_init(c, SST_FBI_INIT4, INIT4);
    tdfx_idle_fbi(c);
    host->pci_write(&c->pci, INIT_ENABLE, INIT_WRITES | FIFO_WRITES);

    if (!(sst.dac = tdfx_dac(c)) || !tdfx_clock(BRING_UP_MHZ, &bring_up))
        return false;

    for (uint32_t t = 0; t < TMUS; t++) {
        tdfx_put_init(c, (SST_TMU0 << t) + TREX_INIT0, TREX0);
        tdfx_idle_fbi(c);
        tdfx_put_init(c, (SST_TMU0 << t) + TREX_INIT1, TREX1);
        tdfx_idle_fbi(c);
    }

    tdfx_dac_graphics(c, sst.dac, &bring_up);
    changed(c, SST_FBI_INIT1, WAIT_STATES, LFB_READS);
    changed(c, SST_FBI_INIT2, SWAP_BITS, 0);
    changed(c, SST_FBI_INIT4, 0, READ_AHEAD);
    changed(c, SST_FBI_INIT2, REFRESH_BITS, REFRESH_RATE);
    changed(c, SST_FBI_INIT2, 0, REFRESH | FAST_MEMORY);
    return true;
}

/* sst1InitVideo: mode m shown, its buffers pages each; true, the clocks set. */
static bool shown_in(Tdfx *const c, const Mode *const m, const uint32_t pages, const uint32_t revision)
{
    const uint32_t ft = revision >= 2 ? 0x5 : 0xA, out_delay = revision >= 2 ? 2 : 0;

    TdfxClock pixel, graphics;

    if (!tdfx_clock(m->mhz, &pixel) || !tdfx_clock(GRAPHICS_MHZ, &graphics))
        return false;

    changed(c, SST_FBI_INIT1, 0, VIDEO_RESET);
    c->host->pci_write(&c->pci, VIDEO_ON, 0);
    tdfx_put_init(c, SST_HSYNC, m->hsync);
    tdfx_put_init(c, SST_VSYNC, m->vsync);
    tdfx_put_init(c, SST_BACK_PORCH, m->back_porch);
    tdfx_put_init(c, SST_DIMENSIONS, (uint32_t)m->h << 16 | (m->w - 1u));

    /* the stride is worked out as fbiInit2 is written: fbiInit1's tiles first */
    changed(c, SST_FBI_INIT1, TILES_BITS, (uint32_t)m->tiles << TILES);
    changed(c, SST_FBI_INIT2, PAGES_BITS, pages << PAGES);
    changed(c, SST_FBI_INIT3, VIDEO_FIFO_BITS | FT_BITS, (uint32_t)m->video_fifo << VIDEO_FIFO | ft << FT_DELAY);
    changed(c, SST_FBI_INIT0, PCI_LWM_BITS | MEMORY_FIFO, PCI_LWM_AT << PCI_LWM);
    changed(c, SST_FBI_INIT1, VIDEO_BITS, OUTPUTS | VIDEO_16BPP | out_delay << OUT_DELAY);
    tdfx_dac_graphics(c, sst.dac, &graphics);
    tdfx_dac_video(c, sst.dac, &pixel);
    changed(c, SST_FBI_INIT1, VIDEO_RESET, 0);
    changed(c, SST_FBI_INIT2, 0, REFRESH);

    tdfx_gamma(c);

    const float clock = tdfx_clock_mhz(&pixel);
    const uint32_t across = (m->hsync & 0xFFFF) + (m->hsync >> 16);
    const uint32_t down = (m->vsync & 0xFFFF) + (m->vsync >> 16);

    c->millihz = (uint32_t)(clock * 1e9f / (float)(across * down));
    return true;
}

static bool opened(Tdfx *const c, const PciFunction *const pci, const uint16_t w, const uint16_t h)
{
    const AccelHost *const host = c->host;
    const Mode *const m = mode_for(w, h);

    if (!m)
        return false;

    host->pci_write(pci, COMMAND, host->pci_read(pci, COMMAND) | MEMORY);

    volatile uint8_t *const regs = host->map(host->pci_read(pci, BAR0) & BAR_MASK, CARD_BYTES);
    const uint32_t revision = host->pci_read(pci, REVISION) & 0xFF;

    if (!regs)
        return false;

    c->regs = regs;
    c->lfb = regs + SST_LFB;
    c->texels = regs + SST_TEXELS;
    sst.shows = tdfx_get(c, SST_FBI_INIT3) & POLARITY && revision > 0 ? 0 : SHOWN;

    const uint32_t stride = m->tiles * TILE, row_bytes = stride * 2;
    const uint32_t most = MB2 / PAGE / 3, by_rows = ROWS_MOST * row_bytes / PAGE;
    const uint32_t pages = most < by_rows ? most : by_rows;

    if (!registered(c, revision) || !shown_in(c, m, pages, revision))
        return false;

    changed(c, SST_FBI_INIT0, SHOWN, sst.shows);
    changed(c, SST_FBI_INIT1, BLANK, 0);

    c->stride = stride;
    c->rows = pages * PAGE / row_bytes;
    c->mode_w = m->w;
    c->mode_h = m->h;
    c->texture_bytes = MB2;
    return true;
}

/* sst1InitShutdown: the VGA's screen shown again. */
static void closed(Tdfx *const c)
{
    TdfxClock shutdown;

    changed(c, SST_FBI_INIT1, 0, VIDEO_RESET | BLANK);
    changed(c, SST_FBI_INIT2, REFRESH, 0);
    reset(c);
    changed(c, SST_FBI_INIT0, SHOWN, sst.shows ^ SHOWN);

    if (tdfx_clock(SHUTDOWN_MHZ, &shutdown))
        tdfx_dac_graphics(c, sst.dac, &shutdown);
}

static const TdfxFamily SST1 = {"Voodoo Graphics", opened, closed, tdfx_carried};

QX_EXPORT const AccelDriver *ACCEL_CALL accel_init(const AccelHost *const host)
{
    return tdfx_driver(host, &SST1);
}
