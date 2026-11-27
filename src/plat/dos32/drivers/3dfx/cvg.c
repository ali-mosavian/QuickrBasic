#include "plat/dos32/drivers/3dfx/cvg.h"
#include "plat/dos32/drivers/3dfx/cmdfifo.h"
#include "plat/dos32/drivers/3dfx/regs.h"
#include "plat/dos32/drivers/3dfx/sst.h"
#include <stddef.h>

enum : uint32_t {
    /* the command FIFO's registers: its first and last pages, the reader's place, the bounds of
       what is written, the words in it, the holes in them */
    FIFO_PAGES = 0x1E0,
    FIFO_READ = 0x1E8,
    FIFO_MIN = 0x1EC,
    FIFO_MAX = 0x1F0,
    FIFO_DEPTH = 0x1F4,
    FIFO_HOLES = 0x1F8,
    LAST_PAGE = 16,
    PAGE = 4096,
    WINDOW = 0x200000, /* BAR0's: the ring as the CPU writes it */
    RING_MOST = 256 << 10,
    /* fbiInit7's: the FIFO on, in memory past the buffers, without hole counting; register writes
       and reads in order with it; when it fetches, its PCI timeout */
    FIFO_ON = 1u << 8,
    OFFSCREEN = 1u << 9,
    NO_HOLES = 1u << 10,
    FETCH = 0x10u << 11,
    FETCH_BITS = 0x1Fu << 11,
    IN_ORDER = 3u << 16,
    TIMEOUT = 127u << 20,
    TIMEOUT_BITS = 0x7Fu << 20,
    /* fbiInit0's: LFB and texture writes through the memory FIFO */
    LFB_MEMORY = 1u << 11,
    TEXTURE_MEMORY = 1u << 12,
};

/* The environment's variable asking for the command FIFO, named as Glide's Voodoo 2 options are. */
static const char ASKED[] = "SSTV2_CMDFIFO";

static CmdFifo queue;

/* An init register's v, a status read after: Glide's, so writes do not burst. */
static void unburst(const Tdfx *const c, const uint32_t reg, const uint32_t v)
{
    tdfx_put_init(c, reg, v);
    tdfx_get(c, SST_STATUS);
}

/* The ring's bytes in spare bytes: a power of two, as the reader wraps it. */
static uint32_t ring_in(const uint32_t spare)
{
    uint32_t bytes = RING_MOST;

    while (bytes > spare)
        bytes >>= 1;

    return bytes;
}

static void put(
    Tdfx *const c,
    const uint32_t reg,
    const TdfxPicks *const p,
    const uint32_t *const v
)
{
    (void)c;
    cmdfifo_registers(&queue, reg, p->mask, v);
}

static uint32_t *corners(Tdfx *const c, const uint32_t n, const bool fan)
{
    const TdfxWay *const w = c->way;

    return cmdfifo_corners(&queue, w->mode, w->corner.n, n, fan);
}

static void uploaded(
    Tdfx *const c,
    const TdfxTexture *const t,
    const AccelRect r,
    const uint16_t *const texels,
    const uint32_t pitch
)
{
    (void)c;
    const uint32_t x = (uint32_t)r.x << (t->indexed ? 0 : SST_TEXEL_AT);
    const uint32_t at = t->level << SST_LEVEL_AT | (uint32_t)r.y << SST_ROW_AT | x;

    cmdfifo_texels(
        &queue,
        at,
        1u << SST_ROW_AT,
        texels,
        pitch,
        (uint32_t)r.w,
        (uint32_t)r.h,
        t->indexed
    );
}

/* The last swap read before another is queued: one may wait on the retrace while the next frame
   is drawn. */
static void shown(Tdfx *const c, const bool vsync)
{
    cmdfifo_waited(&queue);
    tdfx_put(c, SST_SWAP, vsync);
    cmdfifo_marked(&queue);
}

/* As the Voodoo Graphics carries; by the frame buffer's window, the card idle after: its writes
   go by the PCI FIFO, not the command FIFO the draws after them do. */
static void carried(Tdfx *const c)
{
    sst_carried(c);

    if (!c->blits)
        tdfx_idle(c);
}

static const TdfxChip chip = {
    SST_BUSY,
    put,
    sst_based,
    uploaded,
    sst_drawn_in,
    sst_pixels,
    shown,
    carried,
    sst_paletted,
    corners,
    nullptr,
    nullptr,
};

void cvg_queued(Tdfx *const c, const uint32_t pages)
{
    const AccelHost *const host = c->host;

    if (!host->env(ASKED, nullptr, 0))
        return;

    const uint32_t at = sst_after_buffers(pages), bytes = ring_in(c->frame_bytes - at);
    const uint32_t init7 = (tdfx_get(c, SST_FBI_INIT7) & ~(FIFO_ON | OFFSCREEN | NO_HOLES |
                                                           FETCH_BITS | IN_ORDER | TIMEOUT_BITS)) |
                           IN_ORDER | FETCH | TIMEOUT;

    sst_changed(c, SST_FBI_INIT0, SST_MEMORY_FIFO | LFB_MEMORY | TEXTURE_MEMORY, 0);
    unburst(c, SST_FBI_INIT7, init7);
    host->pci_write(&c->pci, SST_INIT_ENABLE, SST_INIT_WRITES);
    unburst(c, FIFO_PAGES, (at + bytes - 1) / PAGE << LAST_PAGE | at / PAGE);
    unburst(c, FIFO_READ, at);
    unburst(c, FIFO_MIN, at - 4);
    unburst(c, FIFO_MAX, at - 4);
    unburst(c, FIFO_DEPTH, 0);
    unburst(c, FIFO_HOLES, 0);
    tdfx_idle_fbi(c);
    unburst(c, SST_FBI_INIT7, init7 | FIFO_ON | OFFSCREEN);
    host->pci_write(&c->pci, SST_INIT_ENABLE, SST_INIT_WRITES | SST_FIFO_WRITES);
    cmdfifo_begin(
        &queue,
        host,
        c->regs + WINDOW,
        (const volatile uint32_t *)(c->regs + FIFO_READ),
        at,
        bytes
    );
    queue.fence = tdfx_combined(c, sst_window(c) + WINDOW, WINDOW) ? c->fence : nullptr;
    c->chip = &chip;
}

void cvg_unqueued(Tdfx *const c)
{
    if (c->chip != &chip)
        return;

    cmdfifo_drained(&queue);
    tdfx_idle_fbi(c);
    tdfx_uncombined(c);
    queue.fence = nullptr;
    sst_changed(c, SST_FBI_INIT7, FIFO_ON, 0);
    sst_changed(c, SST_FBI_INIT0, 0, SST_GRAPHICS_RESET | LFB_MEMORY | TEXTURE_MEMORY);
    sst_stalled(c);
    sst_changed(c, SST_FBI_INIT0, SST_GRAPHICS_RESET, 0);
    c->chip = &sst_chip;
}
