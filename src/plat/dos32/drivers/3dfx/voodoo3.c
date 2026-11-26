/* VOODOO3.DRV: the Banshee and the Voodoo 3 (H3 and H4), shown as Glide does on DOS: a VESA mode
 * of 16 bits from the card's BIOS, its frame memory then drawn in by the 3D unit.
 *
 * # How it works
 * Frame memory holds colour buffers 0 and 1, the depth buffer, then textures, each buffer the
 * mode's rows of its stride and more for targets, linear. The desktop shows a colour buffer by its
 * start address, written in the retrace with vsync, once the card is idle; colBufferAddr sets the
 * one drawn in. The triangle registers are SST-1's at BAR0's 3D window, the command FIFO off.
 * Texels go through TMU0's texture port, linear from texBaseAddr, the texture cache flushed after
 * as Glide does; pixels are read through BAR1, its tiling moved past memory.
 */
#include "plat/dos32/drivers/3dfx/regs.h"
#include "plat/dos32/drivers/3dfx/setup.h"
#include "plat/dos32/drivers/3dfx/tdfx.h"
#include "qx/qx.h"
#include <stddef.h>

/* PCI's: the command register, BAR0 (registers), BAR1 (frame memory). */
enum : uint8_t { COMMAND = 0x04, BAR0 = 0x10, BAR1 = 0x14 };

enum : uint32_t {
    MEMORY = 2, /* PCI command's memory decode */
    BAR_MASK = 0xFFFFFFF0,
    REGS_BYTES = 8 << 20, /* BAR0's io and 3D registers, and TMU0's texture port */
    THREE_D = 0x200000,
    PORT = 0x600000,
    /* io registers */
    LFB_MEMORY = 0x0C, /* lfbMemoryConfig: its tile base's 13 bits, in pages */
    TILE_BASE = 0x1FFF,
    DRAM_INIT0 = 0x18,
    DRAM_INIT1 = 0x1C,
    DESKTOP_START = 0xE4,
    /* dramInit's straps: SDRAM, 16 MB; else SGRAM, 8 chips not 4, of 16 Mbit not 8 */
    SDRAM = 1u << 30,
    EIGHT_CHIPS = 1u << 26,
    SIXTEEN_MBIT = 1u << 27,
    MB = 1 << 20,
    /* 3D registers */
    COLOUR_AT = 0x1EC,
    COLOUR_STRIDE = 0x1F0,
    AUX_AT = 0x1F4,
    AUX_STRIDE = 0x1F8,
    ROOM = 0x1F,
    OUTSIDE = 1u << 6, /* status: not in the vertical retrace */
    BASE = 0xFFFFF0,
    /* frame memory */
    BUFFERS = 3,
    DEPTH = 2,
    ROWS_MOST = 2048, /* the clip's 12 bits */
    PAGE = 4096,
    TEXTURES_LEAST = 1 << 20,
    MEMORY_MOST = 16 << 20, /* texBaseAddr's 24 bits */
    TEXEL = 2,
    PIXEL = 2,
    BITS = 16,
    FRAMES = 4, /* the refresh is timed over */
};

static struct {
    volatile uint8_t *io;
    uint32_t at[BUFFERS]; /* colour buffers 0 and 1 and the depth buffer's first bytes */
    uint32_t textures;    /* texture memory's first byte */
    uint32_t tiles;       /* lfbMemoryConfig as it was */
} h3;

static uint32_t io_get(const uint32_t reg) { return *(const volatile uint32_t *)(h3.io + reg); }

static void io_put(const uint32_t reg, const uint32_t v)
{
    *(volatile uint32_t *)(h3.io + reg) = v;
}

/* Frame memory's bytes by dramInit's straps, as Glide reads them. */
static uint32_t memory_of(const uint32_t dram0, const uint32_t dram1)
{
    const uint32_t chips = dram0 & EIGHT_CHIPS ? 8 : 4, mbit = dram0 & SIXTEEN_MBIT ? 16 : 8;

    return dram1 & SDRAM ? MEMORY_MOST : chips * mbit / 8 * MB;
}

/* The buffers and textures placed in bytes of frame memory for mode m: false, they do not fit. */
static bool laid_out(Tdfx *const c, const AccelMode *const m, const uint32_t bytes)
{
    const uint32_t most =
        bytes > TEXTURES_LEAST ? (bytes - TEXTURES_LEAST) / BUFFERS & ~(PAGE - 1) : 0;
    const uint32_t fit = m->pitch ? most / m->pitch : 0;
    const uint32_t rows = fit < ROWS_MOST ? fit : ROWS_MOST;
    const uint32_t buffer = (rows * m->pitch + PAGE - 1) & ~(PAGE - 1);

    if (rows < m->h)
        return false;

    for (uint32_t b = 0; b < BUFFERS; b++)
        h3.at[b] = b * buffer;

    h3.textures = BUFFERS * buffer;
    c->lfb_row = m->pitch;
    c->stride = m->pitch / PIXEL;
    c->rows = rows;
    c->mode_w = m->w;
    c->mode_h = m->h;
    c->texture_bytes = bytes - h3.textures;
    return true;
}

/* The time the next vertical retrace began to *at: false, none came. */
static bool retraced(const Tdfx *const c, uint64_t *const at)
{
    const AccelHost *const host = c->host;
    const uint64_t until = host->clock_us() + TDFX_WAIT_US;

    while (!(tdfx_get(c, SST_STATUS) & OUTSIDE) && host->clock_us() < until)
        ;
    while (tdfx_get(c, SST_STATUS) & OUTSIDE && host->clock_us() < until)
        ;

    *at = host->clock_us();
    return *at < until;
}

/* The refresh in millihertz: 0, unknown. */
static uint32_t timed(const Tdfx *const c)
{
    uint64_t from, to;

    if (!retraced(c, &from))
        return 0;

    for (uint32_t i = 0; i < FRAMES; i++)
        if (!retraced(c, &to))
            return 0;

    return (uint32_t)(FRAMES * 1e9f / (float)(uint32_t)(to - from));
}

static uint32_t based(const Tdfx *const c, const uint32_t at)
{
    (void)c;
    return (h3.textures + at) & BASE;
}

static void uploaded(
    Tdfx *const c,
    const TdfxTexture *const t,
    const AccelRect r,
    const uint16_t *const texels,
    const uint32_t pitch
)
{
    /* the port is texBaseAddr's: its level past the ones above it */
    volatile uint8_t *const level = c->texels + tdfx_skipped(t->w, t->h);

    for (uint32_t y = 0; y < (uint32_t)r.h; y++) {
        const uint16_t *const row = texels + (size_t)y * pitch;
        volatile uint8_t *const to = level + ((uint32_t)(r.y + (int32_t)y) * t->w + r.x) * TEXEL;

        for (uint32_t s = 0; s < (uint32_t)r.w; s += 2)
            tdfx_store(c, to + s * TEXEL, row[s] | (uint32_t)row[s + 1] << 16);
    }

    /* the texture cache is flushed by a change of base */
    tdfx_put(c, SST_TMU0 + SST_TEX_BASE, ~t->base & BASE);
    tdfx_put(c, SST_NOP, 0);
    tdfx_put(c, SST_TMU0 + SST_TEX_BASE, t->base);
}

static uint32_t drawn_in(Tdfx *const c, const uint32_t buffer)
{
    tdfx_put(c, COLOUR_AT, h3.at[buffer]);
    return 0;
}

static volatile uint8_t *pixels(Tdfx *const c, const uint32_t buffer)
{
    return c->lfb + h3.at[buffer];
}

static void shown(Tdfx *const c, const bool vsync)
{
    uint64_t at;

    tdfx_idle(c);

    if (vsync)
        retraced(c, &at);

    io_put(DESKTOP_START, h3.at[c->shown ^ 1]);
}

/* Through BAR1, a pair of pixels at a time. */
static void carried(Tdfx *const c)
{
    const volatile uint8_t *const from = c->lfb + h3.at[c->shown];
    volatile uint8_t *const to = c->lfb + h3.at[c->shown ^ 1];

    tdfx_idle(c);

    for (uint32_t y = c->top; y < c->top + c->h; y++)
        for (uint32_t x = c->left & ~1u; x < c->left + c->w; x += 2) {
            const uint32_t at = y * c->lfb_row + x * PIXEL;
            *(volatile uint32_t *)(to + at) = *(const volatile uint32_t *)(from + at);
        }
}

static const TdfxChip H3 = {ROOM, based, uploaded, drawn_in, pixels, shown, carried};

static bool opened(Tdfx *const c, const uint16_t w, const uint16_t h)
{
    const AccelHost *const host = c->host;

    host->pci_write(&c->pci, COMMAND, host->pci_read(&c->pci, COMMAND) | MEMORY);

    volatile uint8_t *const regs = host->map(host->pci_read(&c->pci, BAR0) & BAR_MASK, REGS_BYTES);

    if (!regs)
        return false;

    h3.io = regs;
    const uint32_t bytes = memory_of(io_get(DRAM_INIT0), io_get(DRAM_INIT1));
    volatile uint8_t *const frame = host->map(host->pci_read(&c->pci, BAR1) & BAR_MASK, bytes);

    AccelMode m;

    if (!frame || !host->vesa(w, h, BITS, &m) || !laid_out(c, &m, bytes))
        return false;

    c->regs = regs + THREE_D;
    c->texels = regs + PORT;
    c->lfb = frame;
    h3.tiles = io_get(LFB_MEMORY);
    io_put(LFB_MEMORY, h3.tiles | TILE_BASE);
    io_put(DESKTOP_START, h3.at[0]);
    tdfx_put(c, COLOUR_STRIDE, m.pitch);
    tdfx_put(c, AUX_STRIDE, m.pitch);
    tdfx_put(c, AUX_AT, h3.at[DEPTH]);
    c->millihz = timed(c);
    return true;
}

/* The desktop at buffer 0 and BAR1's tiling as they were: the host's mode set does the rest. */
static void closed(Tdfx *const c)
{
    (void)c;
    io_put(DESKTOP_START, h3.at[0]);
    io_put(LFB_MEMORY, h3.tiles);
}

static const TdfxFamily FAMILY = {"Banshee and Voodoo 3", &H3, opened, closed};

QX_EXPORT const AccelDriver *ACCEL_CALL accel_init(const AccelHost *const host)
{
    return tdfx_driver(host, &FAMILY);
}
