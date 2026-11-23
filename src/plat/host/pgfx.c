/* gfx's screen in memory: the terminal's, the window's and the browser's (plat/pgfx.h). */
#include "plat/pgfx.h"
#include "plat/pmem.h"
#include <string.h>

static struct {
    const PgfxPlayer *player, *playing; /* the display's; the open screen's */
    uint8_t *page, *shown;              /* w by h each, null none open */
    uint16_t w, h;
    uint8_t depth, bytes; /* bytes a pixel */
    bool showing;
    uint32_t palette[256];
} screen;

/* A depth's bytes a pixel: 0, none shown. */
static uint8_t bytes_of(const uint8_t depth)
{
    return depth == 8 ? 1 : depth == 15 || depth == 16 ? 2 : depth == 32 ? 4 : 0;
}

void pgfx_set_player(const PgfxPlayer *const p) { screen.player = p; }

const PgfxPlayer *pgfx_open_played(const uint16_t w, const uint16_t h, const uint8_t shaders)
{
    pgfx_close();

    if (screen.player && screen.player->shaders & shaders && screen.player->open(w, h))
        screen.playing = screen.player;

    return screen.playing;
}

const PgfxPlayer *pgfx_playing(void) { return screen.playing; }

bool pgfx_open(const uint16_t w, const uint16_t h, const uint8_t depth)
{
    pgfx_close();

    if (!bytes_of(depth) || !w || !h)
        return false;

    const size_t bytes = (size_t)w * h * bytes_of(depth);
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
    screen.depth = depth;
    screen.bytes = bytes_of(depth);
    return true;
}

void pgfx_close(void)
{
    if (screen.playing)
        screen.playing->close();

    screen.playing = nullptr;
    pmem_free(screen.page);
    pmem_free(screen.shown);
    screen.page = screen.shown = nullptr;
    screen.showing = false;
}

uint8_t *pgfx_page(uint32_t *const pitch)
{
    *pitch = (uint32_t)screen.w * screen.bytes;
    return screen.page;
}

void pgfx_show(const uint32_t *const palette)
{
    if (!screen.page)
        return;

    memcpy(screen.shown, screen.page, (size_t)screen.w * screen.h * screen.bytes);
    memcpy(screen.palette, palette, sizeof screen.palette);
    screen.showing = true;
}

bool pgfx_shown(PgfxFrame *const f)
{
    if (!screen.showing)
        return false;

    *f = (PgfxFrame){screen.shown, screen.w, screen.h, screen.depth, screen.palette};

    return true;
}
