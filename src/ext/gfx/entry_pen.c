/* gfx.pen's entries: where drawing goes and the numbers its flags use (pen.bas). */
#include "gfx.h"

void gfx_pen_target(const int32_t s)
{
    Gx *const g = gx_get();

    if (g && gx_surface(g, (uint32_t)s))
        g->pen.target = (uint32_t)s;
}

void gfx_pen_clip(const int32_t x, const int32_t y, const int32_t w, const int32_t h)
{
    Gx *const g = gx_get();

    if (!g)
        return;

    if (w < 0 || h < 0) {
        gx_fail(ERR_ILLEGAL);
        return;
    }

    g->pen.clip = (Rect){x, y, w, h};

    g->pen.clipped = true;
}

void gfx_pen_unclip(void)
{
    Gx *const g = gx_get();

    if (g)
        g->pen.clipped = false;
}

void gfx_pen_reset(void)
{
    Gx *const g = gx_get();

    if (g)
        pen_reset(&g->pen, GFX_SURFACE_CANVAS);
}

/* 0, the built-in 8 by 8: draw.new_font's sheets come later. */
void gfx_pen_font(const int32_t handle)
{
    Gx *const g = gx_get();

    if (g && handle)
        gx_fail(ERR_UNAVAILABLE);
}

/* Not yet: light, tint, blending and palette rows come with the rest of 2D. */
void gfx_pen_light(const int32_t level) { (void)level, gx_fail(ERR_UNAVAILABLE); }

void gfx_pen_tint(const int32_t colour) { (void)colour, gx_fail(ERR_UNAVAILABLE); }

void gfx_pen_alpha(const int32_t level) { (void)level, gx_fail(ERR_UNAVAILABLE); }

void gfx_pen_recolor(const int32_t row) { (void)row, gx_fail(ERR_UNAVAILABLE); }
