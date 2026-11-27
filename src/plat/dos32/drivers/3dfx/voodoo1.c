/* VOODOO1.DRV: the Voodoo Graphics (SST-1), brought up and shown as Glide does
 * (glide3x/sst1/init/initvg: sst1InitRegisters, sst1InitVideo, sst1InitShutdown).
 *
 * # How it works
 * Its modes are Glide's: 512x384, 640x400 and 640x480, the screen centred in the smallest holding
 * it. The memory FIFO, past the buffers, takes the PCI FIFO's overflow. A board whose fbiInit3
 * bit 12 is set at power-on shows the VGA with fbiInit0 bit 0 set, not clear.
 */
#include "plat/dos32/drivers/3dfx/dac.h"
#include "plat/dos32/drivers/3dfx/regs.h"
#include "plat/dos32/drivers/3dfx/sst.h"
#include "qx/qx.h"

enum : uint32_t {
    POLARITY = 1u << 12, /* fbiInit3's, read at power-on */
    /* fbiInit1's: the outputs on, 16 bits a pixel out, the clock's out delay */
    OUTPUTS = 0xFu << 13,
    VIDEO_16BPP = 3u << 20,
    OUT_DELAY = 27,
    /* fbiInit2's: fast RAS, reads ahead of writes, the read-ahead FIFO */
    FAST_MEMORY = 1u << 5 | 1u << 7 | 1u << 21,
    /* fbiInit3's: the TMU to FBI FIFO's threshold */
    TF_FIFO = 8u << 17,
    /* the init registers as reset */
    INIT0 = 0x410,
    INIT1 = 0x00201102,
    INIT2 = 0x80000040,
    INIT4 = 0x1,
    TMUS = 3,
};

static const float GRAPHICS_MHZ = 50, BRING_UP_MHZ = 16, SHUTDOWN_MHZ = 30;

static const SstMode MODES[] = {
    {512, 384, 0x024F0033, 0x01AE0003, 0x00190037, 16, 23, 0, 20.093f},
    {640, 400, 0x02C00060, 0x01BF0002, 0x00230030, 20, 21, 0, 25.175f},
    {640, 480, 0x02C00060, 0x020B0002, 0x00190026, 20, 23, 0, 25.175f},
};

static struct {
    TdfxDac dac;
    uint32_t shows; /* fbiInit0's bit 0 with the Voodoo shown */
} sst;

/* sst1InitRegisters: the chips reset to Glide's values, the DAC found, the DRAM refreshed. */
static bool registered(Tdfx *const c, const uint32_t revision)
{
    const AccelHost *const host = c->host;
    const uint32_t init3 = (revision >= 2 ? 0x8u : 0xAu) << SST_FT_DELAY | TF_FIFO;

    TdfxClock bring_up;

    host->pci_write(&c->pci, SST_VIDEO_OFF, 0);
    host->pci_write(&c->pci, SST_INIT_ENABLE, SST_INIT_WRITES);
    host->pci_write(&c->pci, SST_SNOOP0, 0);
    host->pci_write(&c->pci, SST_SNOOP1, 0);
    sst_stalled(c);
    tdfx_put_init(c, SST_FBI_INIT3, init3);
    tdfx_put_init(c, SST_FBI_INIT1, tdfx_get(c, SST_FBI_INIT1) | SST_VIDEO_RESET);
    sst_stalled(c);
    sst_reset(c);
    tdfx_put_init(c, SST_FBI_INIT0, INIT0);
    tdfx_put_init(c, SST_FBI_INIT1, INIT1);
    tdfx_put_init(c, SST_FBI_INIT2, INIT2);
    tdfx_put_init(c, SST_FBI_INIT3, init3);
    tdfx_put_init(c, SST_FBI_INIT4, INIT4);
    tdfx_idle_fbi(c);
    host->pci_write(&c->pci, SST_INIT_ENABLE, SST_INIT_WRITES | SST_FIFO_WRITES);

    if (!(sst.dac = tdfx_dac(c)) || !tdfx_clock(BRING_UP_MHZ, &bring_up))
        return false;

    for (uint32_t t = 0; t < TMUS; t++) {
        tdfx_put_init(c, (SST_TMU0 << t) + SST_TREX_INIT0, SST_TREX0);
        tdfx_idle_fbi(c);
        tdfx_put_init(c, (SST_TMU0 << t) + SST_TREX_INIT1, SST_TREX1);
        tdfx_idle_fbi(c);
    }

    tdfx_dac_graphics(c, sst.dac, &bring_up);
    sst_changed(c, SST_FBI_INIT1, SST_WAIT_STATE, SST_LFB_READS);
    sst_changed(c, SST_FBI_INIT2, SST_SWAP_BITS, 0);
    sst_changed(c, SST_FBI_INIT4, 0, SST_READ_AHEAD);
    sst_changed(c, SST_FBI_INIT2, SST_REFRESH_BITS, SST_REFRESH_RATE);
    sst_changed(c, SST_FBI_INIT2, 0, SST_REFRESH | FAST_MEMORY);
    return true;
}

/* sst1InitVideo: mode m shown, its buffers pages each; false, its clocks out of range. */
static bool shown_in(
    Tdfx *const c,
    const SstMode *const m,
    const uint32_t pages,
    const uint32_t revision
)
{
    const uint32_t ft = revision >= 2 ? 0x5 : 0xA, out_delay = revision >= 2 ? 2 : 0;

    TdfxClock pixel, graphics;

    if (!tdfx_clock(m->mhz, &pixel) || !tdfx_clock(GRAPHICS_MHZ, &graphics))
        return false;

    c->host->pci_write(&c->pci, SST_VIDEO_ON, 0);
    sst_timed(c, m);

    /* the stride is worked out as fbiInit2 is written: fbiInit1's tiles first */
    sst_changed(c, SST_FBI_INIT1, SST_TILES_BITS, (uint32_t)m->tiles / 2 << SST_TILES);
    sst_changed(c, SST_FBI_INIT2, SST_PAGES_BITS, pages << SST_PAGES);
    sst_changed(
        c,
        SST_FBI_INIT3,
        SST_VIDEO_FIFO_BITS | SST_FT_BITS,
        (uint32_t)m->video_fifo << SST_VIDEO_FIFO | ft << SST_FT_DELAY
    );
    sst_queued_in_memory(c, pages);
    sst_changed(c, SST_FBI_INIT1, SST_VIDEO_BITS, OUTPUTS | VIDEO_16BPP | out_delay << OUT_DELAY);
    tdfx_dac_graphics(c, sst.dac, &graphics);
    tdfx_dac_video(c, sst.dac, &pixel);
    sst_changed(c, SST_FBI_INIT1, SST_VIDEO_RESET, 0);
    sst_changed(c, SST_FBI_INIT2, 0, SST_REFRESH);
    tdfx_gamma(c);
    sst_laid_out(c, m, pages, tdfx_clock_mhz(&pixel));
    return true;
}

static bool opened(Tdfx *const c, const uint16_t w, const uint16_t h)
{
    const SstMode *const m = sst_mode_for(MODES, sizeof MODES / sizeof *MODES, w, h);

    if (!m || !sst_mapped(c))
        return false;

    const uint32_t revision = c->host->pci_read(&c->pci, SST_REVISION) & 0xFF;
    const uint32_t pages = sst_pages(m);

    sst.shows = tdfx_get(c, SST_FBI_INIT3) & POLARITY && revision > 0 ? 0 : SST_SHOWN;

    if (!registered(c, revision) || !shown_in(c, m, pages, revision))
        return false;

    sst_changed(c, SST_FBI_INIT0, SST_SHOWN, sst.shows);
    sst_changed(c, SST_FBI_INIT1, SST_BLANK, 0);
    c->texture_bytes = SST_BYTES;
    c->packer = c->texels + SST_PACKER;
    tdfx_combined(c, sst_window(c), SST_WINDOW_BYTES); /* the whole window, as sst1InitCachingOn */
    return true;
}

/* sst1InitShutdown: the VGA's screen shown again, the graphics clock Glide leaves. */
static void closed(Tdfx *const c)
{
    TdfxClock shutdown;

    tdfx_uncombined(c);
    sst_closed(c, sst.shows);

    if (tdfx_clock(SHUTDOWN_MHZ, &shutdown))
        tdfx_dac_graphics(c, sst.dac, &shutdown);

    sst_stopped(c);
}

static const TdfxFamily SST1 = {"Voodoo Graphics", &sst_chip, false, opened, closed};

QX_EXPORT const uint32_t accel_ids[] = {ACCEL_ID(TDFX_VENDOR, 0x0001), 0};

QX_EXPORT const AccelDriver *ACCEL_CALL accel_init(const AccelHost *const host)
{
    return tdfx_driver(host, &SST1);
}
