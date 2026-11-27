/* VOODOO3.DRV: the Banshee and the Voodoo 3 (H3 and H4), shown as Glide does on DOS: a VESA mode
 * of 16 bits from the card's BIOS, its frame memory then drawn in by the 3D unit.
 *
 * # How it works
 * Frame memory holds colour buffers 0 and 1, the depth buffer, the command FIFO, then textures,
 * each buffer the mode's rows of its stride and more for targets, linear. Registers and texels go
 * through the command FIFO (cmdfifo.h), as Glide sends them; texels to TMU0's texture port, linear
 * from texBaseAddr, the texture cache flushed after as Glide does. A 565 overlay over the whole
 * mode shows a colour buffer through a CLUT that widens it as gfx does: swapbufferCMD, in the
 * FIFO, moves it to leftOverlayBuf's, at the retrace with vsync; a show waits until the card has
 * read the last. colBufferAddr sets the buffer drawn in. Pixels are read through BAR1, its tiling
 * past memory; a frame is carried by the 2D engine.
 */
#include "plat/dos32/drivers/3dfx/cmdfifo.h"
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
    REGS_BYTES = 4 << 20,    /* BAR0's io, command FIFO, 2D and 3D registers */
    TWO_D_STATUS = 0x100000, /* status, in the 2D registers' window */
    THREE_D = 0x200000,
    /* io registers */
    LFB_MEMORY = 0x0C, /* lfbMemoryConfig: its tile base's 13 bits, in pages */
    TILE_BASE = 0x1FFF,
    DRAM_INIT0 = 0x18,
    DRAM_INIT1 = 0x1C,
    DAC_ADDR = 0x50,
    DAC_DATA = 0x54,
    VID_PROC = 0x5C,
    OVERLAY_START = 0x9C,
    OVERLAY_END = 0xA0,
    OVERLAY_DUDX = 0xA4,
    OVERLAY_WIDTH = 0xA8,
    OVERLAY_DVDY = 0xAC,
    OVERLAY_DVDY_OFFSET = 0xE0,
    STRIDES = 0xE8, /* vidDesktopOverlayStride: the desktop's below, the overlay's from */
    OVERLAY_STRIDE = 16,
    OVERLAY_STRIDE_BITS = 0x7FFFu << OVERLAY_STRIDE,
    /* vidProcCfg's: the overlay on, its CLUT bypassed, its CLUT's upper half, scaled, filtered, its
       format (565), tiled */
    OVERLAY = 1u << 8,
    OVERLAY_BYPASS = 1u << 11,
    OVERLAY_UPPER = 1u << 13,
    OVERLAY_SCALED = 3u << 14,
    FILTER = 3u << 16,
    OVERLAY_FORMAT = 7u << 21,
    OVERLAY_565 = 1u << 21,
    OVERLAY_TILED = 1u << 25,
    END_Y = 12,        /* vidOverlayEndScreenCoord's y */
    SOURCE_WIDTH = 19, /* vidOverlayDudxOffsetSrcWidth's bytes a row */
    ONE = 1u << 20,    /* a step of a pixel, unscaled */
    CLUT = 256,
    /* dramInit's straps: SDRAM, 16 MB; else SGRAM, 8 chips not 4, of 16 Mbit not 8 */
    SDRAM = 1u << 30,
    EIGHT_CHIPS = 1u << 26,
    SIXTEEN_MBIT = 1u << 27,
    MB = 1 << 20,
    /* the command FIFO's registers, from CMD: its base page, its pages less one and its switch, its
       reader's place and the high half of it, the bounds of what is written, the words in it, the
       holes in them; when it fetches */
    CMD = 0x80000,
    FIFO_BASE = 0x20,
    FIFO_SIZE = 0x24,
    FIFO_READ = 0x2C,
    FIFO_READ_HIGH = 0x30,
    FIFO_MIN = 0x34,
    FIFO_MAX = 0x3C,
    FIFO_DEPTH = 0x44,
    FIFO_HOLES = 0x48,
    FIFO_THRESH = 0x80,
    FIFO_ON = 1u << 8,
    FIFO_BYTES = 256 << 10,
    FIFO_BUSY = 1u << 11, /* status: words in the command FIFO */
    BANSHEE = 3,          /* PCI's device: its thresholds Glide's, and the Voodoo 3's */
    BANSHEE_THRESH = 9u << 5 | 2,
    VOODOO3_THRESH = 0xFu << 5 | 8,
    /* 3D registers */
    LEFT_OVERLAY = 0x250,
    COLOUR_AT = 0x1EC,
    COLOUR_STRIDE = 0x1F0,
    AUX_AT = 0x1F4,
    AUX_STRIDE = 0x1F8,
    OUTSIDE = 1u << 6, /* status: not in the vertical retrace */
    BASE = 0xFFFFF0,
    /* 2D registers, PKT1's from TWO_D: clip0Min, clip0Max, dstBaseAddr and dstFormat;
       srcBaseAddr and commandExtra; srcFormat, srcSize and srcXY; dstSize, dstXY and command.
       Clip 0 the most, 16 bits a pixel; a copy of the source, screen to screen, launched. */
    TWO_D = 0x2000,
    BLIT_CLIP = TWO_D + 0x08,
    BLIT_SOURCE = TWO_D + 0x34,
    BLIT_FROM = TWO_D + 0x54,
    BLIT_TO = TWO_D + 0x68,
    CLIP_MOST = 0x0FFF0FFF,
    SIXTEEN_BITS = 3u << 16,
    BLIT = 1 | 1u << 8 | 0xCCu << 24,
    Y_AT = 16,
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
    uint32_t at[BUFFERS];           /* colour buffers 0 and 1 and the depth buffer's first bytes */
    uint32_t fifo, textures;        /* the command FIFO's and texture memory's first bytes */
    uint32_t tiles, video, strides; /* lfbMemoryConfig, vidProcCfg, the strides as they were */
    CmdFifo queue;
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
    const uint32_t least = TEXTURES_LEAST + FIFO_BYTES;
    const uint32_t most = bytes > least ? (bytes - least) / BUFFERS & ~(PAGE - 1) : 0;
    const uint32_t fit = m->pitch ? most / m->pitch : 0;
    const uint32_t rows = fit < ROWS_MOST ? fit : ROWS_MOST;
    const uint32_t buffer = (rows * m->pitch + PAGE - 1) & ~(PAGE - 1);

    if (rows < m->h)
        return false;

    for (uint32_t b = 0; b < BUFFERS; b++)
        h3.at[b] = b * buffer;

    h3.fifo = BUFFERS * buffer;
    h3.textures = h3.fifo + FIFO_BYTES;
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

/* The command FIFO on and empty after the buffers: Glide's hwcInitFifo. */
static void queued(const bool banshee)
{
    io_put(CMD + FIFO_SIZE, 0);
    io_put(CMD + FIFO_BASE, h3.fifo / PAGE);
    io_put(CMD + FIFO_READ, h3.fifo);
    io_put(CMD + FIFO_READ_HIGH, 0);
    io_put(CMD + FIFO_MIN, h3.fifo - 4);
    io_put(CMD + FIFO_MAX, h3.fifo - 4);
    io_put(CMD + FIFO_DEPTH, 0);
    io_put(CMD + FIFO_HOLES, 0);
    io_put(CMD + FIFO_THRESH, banshee ? BANSHEE_THRESH : VOODOO3_THRESH);
    io_put(CMD + FIFO_SIZE, (FIFO_BYTES / PAGE - 1) | FIFO_ON);
}

static void put(
    Tdfx *const c,
    const uint32_t reg,
    const TdfxPicks *const p,
    const uint32_t *const v
)
{
    (void)c;
    cmdfifo_registers(&h3.queue, reg, p->mask, v);
}

static void cornered(Tdfx *const c, const TdfxVertex *const v, const uint32_t n)
{
    cmdfifo_corners(&h3.queue, c->mode, c->corner.at, c->corner.n, v->word, TDFX_VERTEX_WORDS, n);
}

static uint32_t based(const Tdfx *const c, const uint32_t at)
{
    (void)c;
    return (h3.textures + at) & BASE;
}

/* The texture cache flushed by a change of base. */
static void flushed(Tdfx *const c, const uint32_t base)
{
    tdfx_put(c, SST_TMU0 + SST_TEX_BASE, ~base & BASE);
    tdfx_put(c, SST_NOP, 0);
    tdfx_put(c, SST_TMU0 + SST_TEX_BASE, base);
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
    const uint32_t texel = t->indexed ? 1 : TEXEL;
    const uint32_t at =
        tdfx_skipped(t->w, t->h, texel) + ((uint32_t)r.y * t->w + (uint32_t)r.x) * texel;

    cmdfifo_texels(
        &h3.queue,
        at,
        t->w * texel,
        texels,
        pitch,
        (uint32_t)r.w,
        (uint32_t)r.h,
        t->indexed
    );
    flushed(c, t->base);
}

/* The texture cache holds texels looked up in the palette: 86Box's drew an indexed texture
   through the palette before, all 0, in 2 of 4 runs. */
static void paletted(Tdfx *const c)
{
    if (c->placed < ACCEL_TEXTURES)
        flushed(c, c->textures[c->placed].base);
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

/* The overlay moved to buffer's, at the retrace with vsync, once the card has drawn what is
   queued before it. */
static void swapped(Tdfx *const c, const uint32_t buffer, const bool vsync)
{
    tdfx_put(c, LEFT_OVERLAY, h3.at[buffer]);
    tdfx_put(c, SST_SWAP, vsync);
    cmdfifo_fenced(&h3.queue);
}

/* The last swap read before another is queued: one may wait on the retrace while the next frame
   is drawn. */
static void shown(Tdfx *const c, const bool vsync)
{
    cmdfifo_waited(&h3.queue);
    swapped(c, c->shown ^ 1, vsync);
}

/* CLUT entry i: a channel looked up as its 5 or 6 bits with zeros below, or repeated below as gfx
   widens it, shown as gfx widens it. */
static uint32_t widened(const uint32_t i)
{
    const uint32_t five = i | i >> 5, six = i | i >> 6;

    return five << 16 | six << 8 | five;
}

/* The overlay over the whole mode m, unscaled, showing buffer 0. */
static void overlaid(Tdfx *const c, const AccelMode *const m)
{
    for (uint32_t i = 0; i < CLUT; i++) {
        io_put(DAC_ADDR, i);
        io_get(DAC_ADDR); /* tdfxfb's: the address taken before the data */
        io_put(DAC_DATA, widened(i));
    }

    io_put(OVERLAY_START, 0);
    io_put(OVERLAY_END, (m->h - 1u) << END_Y | (m->w - 1u));
    io_put(OVERLAY_DUDX, ONE);
    io_put(OVERLAY_WIDTH, (uint32_t)m->w * PIXEL << SOURCE_WIDTH);
    io_put(OVERLAY_DVDY, ONE);
    io_put(OVERLAY_DVDY_OFFSET, 0);
    io_put(STRIDES, (h3.strides & ~OVERLAY_STRIDE_BITS) | m->pitch << OVERLAY_STRIDE);
    swapped(c, 0, false);
    io_put(
        VID_PROC,
        (h3.video & ~(OVERLAY_BYPASS | OVERLAY_UPPER | OVERLAY_SCALED | FILTER | OVERLAY_FORMAT |
                      OVERLAY_TILED)) |
            OVERLAY | OVERLAY_565
    );
}

/* By the 2D engine, behind the swap in the command FIFO. */
static void carried(Tdfx *const c)
{
    const uint32_t format = c->lfb_row | SIXTEEN_BITS;
    const uint32_t at = (uint32_t)c->top << Y_AT | c->left;
    const uint32_t size = (uint32_t)c->h << Y_AT | c->w;
    const uint32_t clip[] = {0, CLIP_MOST, h3.at[c->shown ^ 1], format};
    const uint32_t source[] = {h3.at[c->shown], 0};
    const uint32_t from[] = {format, size, at};
    const uint32_t to[] = {size, at, BLIT};

    tdfx_put_run(c, BLIT_CLIP, clip, sizeof clip / sizeof *clip);
    tdfx_put_run(c, BLIT_SOURCE, source, sizeof source / sizeof *source);
    tdfx_put_run(c, BLIT_FROM, from, sizeof from / sizeof *from);
    tdfx_put_run(c, BLIT_TO, to, sizeof to / sizeof *to);
}

static const TdfxChip H3 = {
    SST_BUSY | FIFO_BUSY,
    put,
    based,
    uploaded,
    drawn_in,
    pixels,
    shown,
    carried,
    paletted,
    cornered,
};

static bool opened(Tdfx *const c, const uint16_t w, const uint16_t h)
{
    const AccelHost *const host = c->host;
    const bool banshee = host->pci_read(&c->pci, 0) >> 16 == BANSHEE;

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
    c->status = (const volatile uint32_t *)(regs + TWO_D_STATUS);
    c->lfb = frame;
    queued(banshee);
    cmdfifo_begin(
        &h3.queue,
        host,
        frame + h3.fifo,
        (const volatile uint32_t *)(regs + CMD + FIFO_READ),
        h3.fifo,
        FIFO_BYTES
    );
    h3.tiles = io_get(LFB_MEMORY);
    h3.video = io_get(VID_PROC);
    h3.strides = io_get(STRIDES);
    io_put(LFB_MEMORY, h3.tiles | TILE_BASE);
    overlaid(c, &m);
    tdfx_put(c, COLOUR_STRIDE, m.pitch);
    tdfx_put(c, AUX_STRIDE, m.pitch);
    tdfx_put(c, AUX_AT, h3.at[DEPTH]);
    c->millihz = timed(c);
    return true;
}

/* The command FIFO off, the video and BAR1's tiling as they were: the host's mode set does the
   rest. */
static void closed(Tdfx *const c)
{
    (void)c;
    io_put(CMD + FIFO_SIZE, 0);
    io_put(VID_PROC, h3.video);
    io_put(STRIDES, h3.strides);
    io_put(LFB_MEMORY, h3.tiles);
}

static const TdfxFamily FAMILY = {"Banshee and Voodoo 3", &H3, true, opened, closed};

/* The Banshee's and the Voodoo 3's. */
QX_EXPORT const uint32_t accel_ids[] = {
    ACCEL_ID(TDFX_VENDOR, 0x0003),
    ACCEL_ID(TDFX_VENDOR, 0x0005),
    0,
};

QX_EXPORT const AccelDriver *ACCEL_CALL accel_init(const AccelHost *const host)
{
    return tdfx_driver(host, &FAMILY);
}
