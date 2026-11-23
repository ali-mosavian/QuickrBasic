/* gfx's screen on DOS (plat/pgfx.h): none yet, until VBE's pages. */
#include "plat/pgfx.h"

void pgfx_set_player(const PgfxPlayer *const p) { (void)p; }

bool pgfx_open_played(const uint16_t w, const uint16_t h)
{
    (void)w, (void)h;
    return false;
}

const PgfxPlayer *pgfx_playing(void) { return nullptr; }

bool pgfx_open(const uint16_t w, const uint16_t h, const uint8_t depth)
{
    (void)w, (void)h, (void)depth;
    return false;
}

void pgfx_close(void) {}

uint8_t *pgfx_page(uint32_t *const pitch)
{
    *pitch = 0;
    return nullptr;
}

void pgfx_show(const uint32_t *const palette) { (void)palette; }

bool pgfx_shown(PgfxFrame *const f)
{
    (void)f;
    return false;
}
