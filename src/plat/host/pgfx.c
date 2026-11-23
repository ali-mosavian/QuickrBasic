/* gfx's screen in memory: the terminal's, the window's and the browser's (plat/pgfx.h). */
#include "plat/pgfx.h"
#include "plat/pmem.h"
#include <string.h>

static struct {
    uint8_t *page, *shown; /* w by h each, null none open */
    uint16_t w, h;
    bool showing;
    uint32_t palette[256];
} screen;

bool pgfx_open(const uint16_t w, const uint16_t h, const uint8_t depth)
{
    pgfx_close();

    if (depth != 8 || !w || !h)
        return false;

    const size_t bytes = (size_t)w * h;
    uint8_t *const page = pmem_resize(nullptr, bytes), *const shown = pmem_resize(nullptr, bytes);

    if (!page || !shown) {
        pmem_free(page);
        pmem_free(shown);
        return false;
    }

    memset(page, 0, bytes);
    screen.page = page;
    screen.shown = shown;
    screen.w = w;
    screen.h = h;
    return true;
}

void pgfx_close(void)
{
    pmem_free(screen.page);
    pmem_free(screen.shown);
    screen.page = screen.shown = nullptr;
    screen.showing = false;
}

uint8_t *pgfx_page(uint32_t *const pitch)
{
    *pitch = screen.w;
    return screen.page;
}

void pgfx_show(const uint32_t *const palette)
{
    if (!screen.page)
        return;

    memcpy(screen.shown, screen.page, (size_t)screen.w * screen.h);
    memcpy(screen.palette, palette, sizeof screen.palette);
    screen.showing = true;
}

bool pgfx_shown(PgfxFrame *const f)
{
    if (!screen.showing)
        return false;

    *f = (PgfxFrame){screen.shown, screen.w, screen.h, screen.palette};

    return true;
}
