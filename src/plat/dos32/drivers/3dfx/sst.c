/* The SST-1's and CVG's bring-up shared (sst.h). */
#include "plat/dos32/drivers/3dfx/sst.h"
#include "plat/dos32/drivers/3dfx/regs.h"

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
};

const SstMode *sst_mode_for(
    const SstMode *const modes,
    const uint32_t n,
    const uint16_t w,
    const uint16_t h,
    const uint32_t bytes
)
{
    const SstMode *best = nullptr;

    for (uint32_t i = 0; i < n; i++) {
        const SstMode *const m = &modes[i];
        const bool fits = sst_pages(m, bytes) * PAGE / (m->tiles * TILE * 2u) >= m->h;
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

    c->regs = regs;
    c->lfb = regs + SST_LFB;
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

uint32_t sst_pages(const SstMode *const m, const uint32_t bytes)
{
    const uint32_t row_bytes = m->tiles * TILE * 2;
    const uint32_t most = bytes / PAGE / BUFFERS, by_rows = ROWS_MOST * row_bytes / PAGE;

    return most < by_rows ? most : by_rows;
}

void sst_laid_out(Tdfx *const c, const SstMode *const m, const uint32_t pages, const float clock)
{
    const uint32_t across = (m->hsync & HALF) + (m->hsync >> HALF_AT);
    const uint32_t down = (m->vsync & HALF) + (m->vsync >> HALF_AT);

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
