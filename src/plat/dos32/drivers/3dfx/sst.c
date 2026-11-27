/* The SST-1's and CVG's bring-up and chip shared (sst.h). */
#include "plat/dos32/drivers/3dfx/sst.h"
#include "plat/dos32/drivers/3dfx/regs.h"
#include "plat/dos32/drivers/3dfx/setup.h"
#include <stddef.h>

enum : uint32_t {
    MEMORY = 2, /* PCI command's memory decode */
    BAR_MASK = 0xFF000000,
    CARD_BYTES = 16 << 20,
    STALLS = 3,
    PAGE = 4096,
    TILE = 32,
    ROWS_MOST = 1024, /* the clip's and the LFB's 10 bits */
    BUFFERS = 3,
    HALF = 0xFFFF,
    HALF_AT = 16,
    FRONT = 0, /* fbzMode's and lfbMode's buffers: the one shown, the other */
    BACK = 1,
    BASE = 0x7FFFF, /* texBaseAddr's: 8 bytes a unit */
    BASE_UNIT = 3,
    LFB_ROW = 2048, /* the frame buffer window's bytes a row */
    PIXEL_AT = 1,
    LFB_READ = 6, /* lfbMode's buffers read and written */
    LFB_WRITE = 4,
    PENDING = 28, /* status's swaps pending */
    SWAPS = 7,
    AHEAD = 1, /* swaps let pend */
    /* the memory FIFO's: fbiInit4's first and last rows, its low-water mark; fbiInit0's high-water
       mark in 32 entries, its burst's; status' entries free; an entry's bytes, the most entries;
       the room Glide keeps over the high-water mark */
    ROW_BASE = 8,
    ROW_ROLL = 18,
    ROWS_BITS = 0xFFFFFu << ROW_BASE,
    MEMORY_LWM = 23u << 2,
    MEMORY_LWM_BITS = 0x3Fu << 2,
    HWM = 14,
    HWM_BITS = 0x7FFu << HWM,
    HWM_UNIT = 5,
    BURST = 54u << 25,
    BURST_BITS = 0x3Fu << 25,
    MEMORY_FREE = 12,
    ENTRY = 8,
    ENTRIES_MOST = 57344,
    MARGIN = 0x100,
    FIFO_LEAST = 32 << 10, /* the frame buffer a FIFO keeps past the buffers */
    /* the Voodoo 2's blitter: bltSrcBaseAddr, bltDstBaseAddr and bltXYStrides; bltSrcXY,
       bltDstXY, bltSize and bltRop; bltCommand. A copy of the source, screen to screen between
       tiled buffers, launched. */
    BLIT_BASES = 0x2C0,
    BLIT_PLACES = 0x2E0,
    BLIT_COMMAND = 0x2F8,
    BLIT_COPY = 0xCCCC,
    BLIT = 3u << 14 | 1u << 31,
    MARK = 0x5A5AA5A5, /* two pixels the blitter is tried on */
};

static uint32_t room;         /* the FIFO's entries known free */
static uint32_t lwm;          /* the memory FIFO's entries free kept: 0, it is off */
static uint32_t buffer_pages; /* a colour buffer's */

const SstMode *sst_mode_for(
    const SstMode *const modes,
    const uint32_t n,
    const uint16_t w,
    const uint16_t h
)
{
    const SstMode *best = nullptr;

    for (uint32_t i = 0; i < n; i++) {
        const SstMode *const m = &modes[i];
        const bool fits = sst_pages(m) * PAGE / (m->tiles * TILE * 2u) >= m->h;
        const bool holds = m->w >= w && m->h >= h;

        if (fits && holds && (!best || (uint32_t)m->w * m->h < (uint32_t)best->w * best->h))
            best = m;
    }

    return best;
}

void sst_changed(const Tdfx *const c, const uint32_t reg, const uint32_t clear, const uint32_t set)
{
    tdfx_put_init(c, reg, (tdfx_get(c, reg) & ~clear) | set);
    tdfx_idle_fbi(c);
}

void sst_stalled(const Tdfx *const c)
{
    for (uint32_t i = 0; i < STALLS; i++)
        tdfx_get(c, SST_STATUS);
}

void sst_reset(const Tdfx *const c)
{
    sst_changed(c, SST_FBI_INIT0, 0, SST_GRAPHICS_RESET | SST_FIFO_RESET);
    sst_changed(c, SST_FBI_INIT0, SST_FIFO_RESET, 0);
    sst_changed(c, SST_FBI_INIT0, SST_GRAPHICS_RESET, 0);
}

bool sst_mapped(Tdfx *const c)
{
    const AccelHost *const host = c->host;

    host->pci_write(&c->pci, SST_COMMAND, host->pci_read(&c->pci, SST_COMMAND) | MEMORY);

    volatile uint8_t *const regs =
        host->map(host->pci_read(&c->pci, SST_BAR0) & BAR_MASK, CARD_BYTES);

    if (!regs)
        return false;

    room = lwm = 0;
    c->regs = regs;
    c->lfb = regs + SST_LFB;
    c->lfb_row = LFB_ROW;
    c->texels = regs + SST_TEXELS;
    return true;
}

void sst_timed(const Tdfx *const c, const SstMode *const m)
{
    sst_changed(c, SST_FBI_INIT1, 0, SST_VIDEO_RESET);
    tdfx_put_init(c, SST_HSYNC, m->hsync);
    tdfx_put_init(c, SST_VSYNC, m->vsync);
    tdfx_put_init(c, SST_BACK_PORCH, m->back_porch);
    tdfx_put_init(c, SST_DIMENSIONS, (uint32_t)m->h << HALF_AT | (m->w - 1u));
    tdfx_idle_fbi(c);
}

uint32_t sst_pages(const SstMode *const m)
{
    const uint32_t row_bytes = m->tiles * TILE * 2;
    const uint32_t most = (SST_BYTES - FIFO_LEAST) / PAGE / BUFFERS,
                   by_rows = ROWS_MOST * row_bytes / PAGE;

    return most < by_rows ? most : by_rows;
}

void sst_laid_out(Tdfx *const c, const SstMode *const m, const uint32_t pages, const float clock)
{
    const uint32_t across = (m->hsync & HALF) + (m->hsync >> HALF_AT);
    const uint32_t down = (m->vsync & HALF) + (m->vsync >> HALF_AT);

    buffer_pages = pages;
    c->stride = m->tiles * TILE;
    c->rows = pages * PAGE / (c->stride * 2);
    c->mode_w = m->w;
    c->mode_h = m->h;
    c->millihz = (uint32_t)(clock * 1e9f / (float)(across * down));
}

void sst_closed(const Tdfx *const c, const uint32_t shows)
{
    sst_changed(c, SST_FBI_INIT1, 0, SST_VIDEO_RESET | SST_BLANK);
    sst_changed(c, SST_FBI_INIT2, SST_REFRESH, 0);
    sst_reset(c);
    sst_changed(c, SST_FBI_INIT0, SST_SHOWN, shows ^ SST_SHOWN);
}

void sst_stopped(const Tdfx *const c)
{
    c->host->pci_write(&c->pci, SST_VIDEO_OFF, 0);
    c->host->pci_write(&c->pci, SST_INIT_ENABLE, 0);
}

uint32_t sst_after_buffers(const uint32_t pages) { return BUFFERS * pages * PAGE; }

void sst_queued_in_memory(const Tdfx *const c, const uint32_t pages)
{
    const uint32_t first = sst_after_buffers(pages) / PAGE, rows = SST_BYTES / PAGE;
    const uint32_t spare = (rows - first - 1) * PAGE / ENTRY;
    const uint32_t entries = spare < ENTRIES_MOST ? spare : ENTRIES_MOST;
    const uint32_t hwm = (HALF - entries + (1u << HWM_UNIT) - 1) >> HWM_UNIT;

    sst_changed(
        c,
        SST_FBI_INIT4,
        ROWS_BITS | MEMORY_LWM_BITS,
        first << ROW_BASE | (rows - 1) << ROW_ROLL | MEMORY_LWM
    );
    sst_changed(
        c,
        SST_FBI_INIT0,
        SST_MEMORY_FIFO | HWM_BITS | SST_PCI_LWM_BITS | BURST_BITS,
        hwm << HWM | SST_PCI_LWM | BURST | SST_MEMORY_FIFO
    );
    lwm = (hwm << HWM_UNIT | ((1u << HWM_UNIT) - 1)) + MARGIN;
}

/* The entries a write may take: the memory FIFO's free over its low-water mark, as Glide's
   _grReCacheFifo; else the PCI FIFO's. */
static uint32_t room_in(const uint32_t status)
{
    const uint32_t free = status >> MEMORY_FREE & HALF;

    return !lwm ? status & SST_ROOM : free > lwm ? free - lwm : 0;
}

/* A word to a window's at, as the FIFO has room. */
static void stored(Tdfx *const c, volatile uint8_t *const at, const uint32_t v)
{
    while (!room)
        room = room_in(tdfx_get(c, SST_STATUS));

    room--;
    *(volatile uint32_t *)at = v;
}

static void put(Tdfx *const c, const uint32_t reg, const uint32_t *const v, const uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        stored(c, c->regs + reg + 4 * i, v[i]);
}

uint32_t sst_based(const Tdfx *const c, const uint32_t at)
{
    (void)c;
    return at >> BASE_UNIT & BASE;
}

static void uploaded(
    Tdfx *const c,
    const TdfxTexture *const t,
    const AccelRect r,
    const uint16_t *const texels,
    const uint32_t pitch
)
{
    volatile uint8_t *const level = c->texels + (t->level << SST_LEVEL_AT);

    for (uint32_t y = 0; y < (uint32_t)r.h; y++) {
        const uint16_t *const row = texels + (size_t)y * pitch;
        volatile uint8_t *const to = level + ((uint32_t)(r.y + (int32_t)y) << SST_ROW_AT);

        for (uint32_t s = 0; s < (uint32_t)r.w; s += t->indexed ? 4 : 2)
            if (t->indexed)
                stored(c, to + r.x + s, tdfx_indices(row + s));
            else
                stored(c, to + ((r.x + s) << SST_TEXEL_AT), row[s] | (uint32_t)row[s + 1] << 16);
    }
}

void sst_paletted(Tdfx *const c) { (void)c; }

/* Colour buffer `buffer` as fbzMode and lfbMode name it. */
static uint32_t side_of(const Tdfx *const c, const uint32_t buffer)
{
    return buffer == c->shown ? FRONT : BACK;
}

uint32_t sst_drawn_in(Tdfx *const c, const uint32_t buffer) { return side_of(c, buffer); }

volatile uint8_t *sst_pixels(Tdfx *const c, const uint32_t buffer)
{
    tdfx_put(c, SST_LFB_MODE, side_of(c, buffer) << LFB_READ);
    return c->lfb;
}

/* At most AHEAD swaps pending, one waiting on the retrace while the next frame is drawn. */
static void shown(Tdfx *const c, const bool vsync)
{
    const uint64_t until = c->host->clock_us() + TDFX_WAIT_US;

    while ((tdfx_get(c, SST_STATUS) >> PENDING & SWAPS) > AHEAD && c->host->clock_us() < until)
        ;

    tdfx_put(c, SST_SWAP, vsync);
}

/* w by h pixels at x, y of the shown buffer copied to the other's by the Voodoo 2's blitter, in
   order with the draws. */
static void blitted(
    Tdfx *const c,
    const uint32_t x,
    const uint32_t y,
    const uint32_t w,
    const uint32_t h
)
{
    const uint32_t tiles = c->stride / TILE, at = y << HALF_AT | x;
    const uint32_t bases[] = {
        c->shown * buffer_pages,
        (c->shown ^ 1) * buffer_pages,
        tiles << HALF_AT | tiles,
    };
    const uint32_t places[] = {at, at, (h - 1) << HALF_AT | (w - 1), BLIT_COPY};

    c->chip->put(c, BLIT_BASES, bases, sizeof bases / sizeof *bases);
    c->chip->put(c, BLIT_PLACES, places, sizeof places / sizeof *places);
    tdfx_put(c, BLIT_COMMAND, BLIT);
}

bool sst_blits(Tdfx *const c)
{
    tdfx_put(c, SST_LFB_MODE, FRONT << LFB_WRITE);
    stored(c, c->lfb, MARK);
    tdfx_put(c, SST_LFB_MODE, BACK << LFB_READ | BACK << LFB_WRITE);
    stored(c, c->lfb, ~MARK);
    blitted(c, 0, 0, 2, 1);
    tdfx_idle(c);
    return *(const volatile uint32_t *)c->lfb == MARK;
}

/* By the blitter; else through the frame buffer's window, pixel by pixel. */
void sst_carried(Tdfx *const c)
{
    if (c->blits) {
        blitted(c, c->left, c->top, c->w, c->h);
        return;
    }

    tdfx_put(c, SST_LFB_MODE, FRONT << LFB_READ | BACK << LFB_WRITE);
    tdfx_idle(c);

    for (uint32_t y = c->top; y < c->top + c->h; y++)
        for (uint32_t x = c->left & ~1u; x < c->left + c->w; x += 2) {
            volatile uint8_t *const at = c->lfb + y * LFB_ROW + (x << PIXEL_AT);
            stored(c, at, *(const volatile uint32_t *)at);
        }
}

const TdfxChip sst_chip = {
    SST_BUSY,
    put,
    sst_based,
    uploaded,
    sst_drawn_in,
    sst_pixels,
    shown,
    sst_carried,
    sst_paletted,
};
