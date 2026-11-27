/* VOODOO2.DRV: the Voodoo 2 (CVG), brought up and shown as Glide does (glide3x/cvg/init:
 * sst1InitRegisters, sst1InitVideoBuffers, sst1InitResetTmus, sst1InitShutdown).
 *
 * # How it works
 * Its modes are Glide's tables, 320x200 to 640x480, the small ones scan doubled; the screen goes
 * centred in the smallest holding it whose buffers fit. Only TMU0 draws. The TMUs are reset after
 * each clock change. Registers and texels are written directly, or through the command FIFO when
 * SSTV2_CMDFIFO is set (cvg.h), on once the mode is shown. PCI register 0xC0 is written once the
 * mode is set: DOSBox-X shows nothing without it, and sizes its window then. A frame is carried
 * by the blitter, once tried; DOSBox-X has none, and the CPU copies.
 */
#include "plat/dos32/drivers/3dfx/cvg.h"
#include "plat/dos32/drivers/3dfx/dac.h"
#include "plat/dos32/drivers/3dfx/regs.h"
#include "plat/dos32/drivers/3dfx/sst.h"
#include "qx/qx.h"

enum : uint32_t {
    TMUS = 2,
    /* fbiInit1's: the top tiles bit, the video's bits for 16 bits a pixel out */
    TILES_TOP = 24,
    VIDEO_16BPP = 0x2031E000,
    /* fbiInit2's: reads ahead of writes, the read-ahead FIFO */
    READ_AHEAD_WRITES = 1u << 7 | 1u << 21,
    /* fbiInit3's: the TMUs off */
    TMUS_OFF = 1u << 6,
    /* fbiInit5's: scan doubling's shift */
    DOUBLED = 20,
    /* fbiInit6's: the lowest tiles bit */
    TILES_BOTTOM = 30,
    /* fbiInit7's: the graphics clock's strap */
    STRAP = 2,
    STRAP_BITS = 0x3F,
    /* the init registers as reset */
    INIT0 = 0x1C10,
    INIT1 = 0x00201102,
    INIT2 = 0x80000040,
    INIT3 = 0x00110040,
    INIT4 = 0x1,
    INIT5 = 0x08408000,
    INIT6 = 0,
    INIT7 = 0x08080000,
    /* trexInit1's: its FIFO and core reset */
    TREX_RESET = 0x180000,
    /* status': the FBI and TMUs busy */
    UNITS_BUSY = 3u << 7,
    BUSY_READS = 50000,
    TRIES = 3,
    DELAY_US = 1000,  /* Glide's: a CPU loop of 10000 */
    CLOCK_US = 20000, /* Glide's: 200000 status reads */
    SLOW_GRX = 45,
    SLOW_FIFO = 4,
    FT = 4, /* the FBI to TMU and TMU to FBI clock delays, Glide's when not measured */
    TF = 6,
    /* a write past the TMUs' memory: Glide's for the first texture download */
    WAKE = 0xF00000,
    WAKE_WORD = 0xDEADBEEF,
};

static const float BRING_UP_MHZ = 16, SHUTDOWN_MHZ = 30, GRAPHICS_MHZ = 50;

static const SstMode MODES[] = {
    {320, 200, 0x02C00060, 0x01BF0002, 0x00230030, 10, 25, 3, 25.175f},
    {320, 240, 0x02C00060, 0x020B0002, 0x00190026, 10, 25, 3, 25.175f},
    {400, 300, 0x01D70027, 0x026B0003, 0x00120036, 14, 23, 2, 19.108f},
    {512, 384, 0x02670037, 0x03180003, 0x0017004E, 16, 23, 2, 32.054f},
    {640, 400, 0x02C00060, 0x01BF0002, 0x00230030, 20, 25, 0, 25.175f},
    {640, 480, 0x02C00060, 0x020B0002, 0x00190026, 20, 25, 0, 25.175f},
    {800, 600, 0x039F007F, 0x02700004, 0x00170056, 26, 23, 0, 40.0f},
};

static struct {
    TdfxDac dac;
    float grx; /* the graphics clock, by the board's strap */
} cvg;

static void waited(const Tdfx *const c, const uint32_t us)
{
    const uint64_t until = c->host->clock_us() + us;

    while (c->host->clock_us() < until)
        ;
}

static void delayed_put(const Tdfx *const c, const uint32_t reg, const uint32_t v)
{
    tdfx_put_init(c, reg, v);
    waited(c, DELAY_US);
}

static void delayed_change(
    const Tdfx *const c,
    const uint32_t reg,
    const uint32_t clear,
    const uint32_t set
)
{
    delayed_put(c, reg, (tdfx_get(c, reg) & ~clear) | set);
}

/* Each TMU's reg set to v. */
static void tmus_put(const Tdfx *const c, const uint32_t reg, const uint32_t v)
{
    for (uint32_t t = 0; t < TMUS; t++)
        delayed_put(c, (SST_TMU0 << t) + reg, v);
}

/* sst1InitResetTmus: the TMUs' FIFOs and cores reset and let run, after a clock change. */
static void tmus_reset(const Tdfx *const c, const uint32_t trex1)
{
    bool calm = false;

    delayed_change(c, SST_FBI_INIT3, 0, TMUS_OFF);

    for (uint32_t try = 0; try < TRIES && !calm; try++) {
        tmus_put(c, SST_TREX_INIT0, SST_TREX0);
        tmus_put(c, SST_TREX_INIT1, trex1);
        tmus_put(c, SST_TREX_INIT1, trex1 | TREX_RESET);
        tmus_put(c, SST_TREX_INIT1, trex1);
        delayed_change(c, SST_FBI_INIT0, 0, SST_GRAPHICS_RESET | SST_FIFO_RESET);
        delayed_change(c, SST_FBI_INIT0, SST_FIFO_RESET, 0);
        delayed_change(c, SST_FBI_INIT0, SST_GRAPHICS_RESET, 0);
        delayed_change(c, SST_FBI_INIT3, TMUS_OFF, 0);

        for (uint32_t i = 0; i < BUSY_READS && !calm; i++)
            calm = !(tdfx_get(c, SST_STATUS) & UNITS_BUSY);
    }

    tdfx_put_init(c, WAKE, WAKE_WORD);
    tdfx_idle_fbi(c);
}

/* The graphics clock set at mhz, the TMUs reset after: false, out of range. */
static bool clocked(Tdfx *const c, const float mhz, const uint32_t trex1)
{
    TdfxClock k;

    if (!tdfx_clock(mhz, &k))
        return false;

    tdfx_dac_graphics(c, cvg.dac, &k);
    tmus_reset(c, trex1);
    return true;
}

/* sst1InitRegisters: the chips reset to Glide's values, the DAC found, the DRAM refreshed. */
static bool registered(Tdfx *const c)
{
    const AccelHost *const host = c->host;
    static const uint32_t defaults[][2] = {
        {SST_FBI_INIT0, INIT0},
        {SST_FBI_INIT1, INIT1},
        {SST_FBI_INIT2, INIT2},
        {SST_FBI_INIT3, INIT3},
        {SST_FBI_INIT4, INIT4},
        {SST_FBI_INIT5, INIT5},
        {SST_FBI_INIT6, INIT6},
        {SST_FBI_INIT7, INIT7},
    };

    host->pci_write(&c->pci, SST_INIT_ENABLE, SST_INIT_WRITES);
    host->pci_write(&c->pci, SST_SNOOP0, 0);
    host->pci_write(&c->pci, SST_SNOOP1, 0);
    sst_stalled(c);
    tdfx_put_init(c, SST_FBI_INIT3, INIT3);
    sst_stalled(c);
    tdfx_put_init(c, SST_FBI_INIT1, tdfx_get(c, SST_FBI_INIT1) | SST_VIDEO_RESET);
    sst_stalled(c);
    sst_reset(c);

    for (uint32_t i = 0; i < sizeof defaults / sizeof *defaults; i++)
        tdfx_put_init(c, defaults[i][0], defaults[i][1]);

    tdfx_idle_fbi(c);
    host->pci_write(&c->pci, SST_INIT_ENABLE, SST_INIT_WRITES | SST_FIFO_WRITES);

    if (!(cvg.dac = tdfx_dac(c)))
        return false;

    for (uint32_t t = 0; t < TMUS; t++) {
        tdfx_put_init(c, (SST_TMU0 << t) + SST_TREX_INIT0, SST_TREX0);
        tdfx_idle_fbi(c);
        tdfx_put_init(c, (SST_TMU0 << t) + SST_TREX_INIT1, SST_TREX1);
        tdfx_idle_fbi(c);
    }

    if (!clocked(c, BRING_UP_MHZ, SST_TREX1))
        return false;

    sst_changed(c, SST_FBI_INIT1, SST_WAIT_STATE, SST_LFB_READS);
    sst_changed(c, SST_FBI_INIT2, SST_SWAP_BITS, 0);
    sst_changed(c, SST_FBI_INIT4, 0, SST_READ_AHEAD);
    sst_changed(c, SST_FBI_INIT2, SST_REFRESH_BITS, SST_REFRESH_RATE);
    sst_changed(c, SST_FBI_INIT2, 0, SST_REFRESH);
    sst_changed(c, SST_FBI_INIT2, 0, READ_AHEAD_WRITES);
    return true;
}

/* The tiles across, in fbiInit6's and fbiInit1's three fields. */
static void tiled(const Tdfx *const c, const uint32_t tiles)
{
    sst_changed(c, SST_FBI_INIT6, 1u << TILES_BOTTOM, (tiles & 1) << TILES_BOTTOM);
    sst_changed(
        c,
        SST_FBI_INIT1,
        SST_TILES_BITS | 1u << TILES_TOP,
        (tiles >> 1 & 0xF) << SST_TILES | (tiles >> 5 & 1) << TILES_TOP
    );
}

/* sst1InitVideoBuffers: mode m shown, its buffers pages each; false, its clocks out of range. */
static bool shown_in(Tdfx *const c, const SstMode *const m, const uint32_t pages)
{
    const uint32_t fifo = m->video_fifo - (cvg.grx < SLOW_GRX ? SLOW_FIFO : 0);
    const uint32_t trex1 = (SST_TREX1 & ~SST_TF_BITS) | TF << SST_TF_DELAY;

    TdfxClock pixel;

    if (!tdfx_clock(m->mhz, &pixel))
        return false;

    sst_timed(c, m);

    /* the stride is worked out as fbiInit2 is written: the tiles first */
    tiled(c, m->tiles);
    sst_changed(
        c,
        SST_FBI_INIT2,
        SST_PAGES_BITS | SST_BANKED,
        pages << SST_PAGES | (c->frame_bytes == 4 << 20 ? SST_BANKED : 0)
    );
    sst_changed(c, SST_FBI_INIT3, SST_VIDEO_FIFO_BITS, fifo << SST_VIDEO_FIFO);
    sst_changed(c, SST_FBI_INIT0, SST_PCI_LWM_BITS | SST_MEMORY_FIFO, SST_PCI_LWM);
    sst_changed(c, SST_FBI_INIT1, SST_VIDEO_BITS, VIDEO_16BPP);
    sst_changed(c, SST_FBI_INIT5, 3u << DOUBLED, (uint32_t)m->flags << DOUBLED);
    sst_changed(c, SST_FBI_INIT3, SST_FT_BITS, FT << SST_FT_DELAY);

    if (!clocked(c, cvg.grx, trex1))
        return false;

    tdfx_dac_video(c, cvg.dac, &pixel);
    waited(c, CLOCK_US);
    sst_changed(c, SST_FBI_INIT1, SST_VIDEO_RESET, 0);
    sst_changed(c, SST_FBI_INIT2, 0, SST_REFRESH);
    tdfx_gamma(c);
    sst_laid_out(c, m, pages, tdfx_clock_mhz(&pixel));
    return true;
}

static void closed(Tdfx *c);

/* A board whose memory answers none of fbiMemSize's tests is taken as every board's least. */
static bool opened(Tdfx *const c, const uint16_t w, const uint16_t h)
{
    if (!sst_mapped(c))
        return false;

    const uint32_t strap = tdfx_get(c, SST_FBI_INIT7) >> STRAP & STRAP_BITS;

    cvg.grx = GRAPHICS_MHZ + (float)strap;

    if (!registered(c))
        return false;

    const uint32_t sized = sst_sized(c, tiled);

    c->frame_bytes = sized ? sized : SST_BYTES;

    const SstMode *const m =
        sst_mode_for(MODES, sizeof MODES / sizeof *MODES, w, h, c->frame_bytes);
    if (!m) {
        closed(c);
        return false;
    }

    const uint32_t pages = sst_pages(m, c->frame_bytes);

    if (!shown_in(c, m, pages))
        return false;

    c->host->pci_write(&c->pci, SST_VIDEO_ON, 0);
    sst_changed(c, SST_FBI_INIT0, 0, SST_SHOWN);
    sst_changed(c, SST_FBI_INIT1, SST_BLANK, 0);
    c->blits = sst_blits(c);
    cvg_queued(c, pages);
    c->texture_bytes = SST_BYTES;
    return true;
}

/* sst1InitShutdown: the VGA's screen shown again, the graphics clock Glide leaves. */
static void closed(Tdfx *const c)
{
    cvg_unqueued(c);
    sst_closed(c, SST_SHOWN);
    clocked(c, SHUTDOWN_MHZ, SST_TREX1);
    sst_stopped(c);
}

static const TdfxFamily CVG = {"Voodoo 2", &sst_chip, true, opened, closed};

QX_EXPORT const uint32_t accel_ids[] = {ACCEL_ID(TDFX_VENDOR, 0x0002), 0};

QX_EXPORT const AccelDriver *ACCEL_CALL accel_init(const AccelHost *const host)
{
    return tdfx_driver(host, &CVG);
}
