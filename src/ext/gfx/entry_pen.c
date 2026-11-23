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

void gfx_pen_font(const int32_t handle)
{
    Gx *const g = gx_get();

    if (!g)
        return;

    if (!font_get(&g->fonts, (uint32_t)handle)) {
        gx_fail(ERR_ILLEGAL);
        return;
    }

    g->pen.font = (uint32_t)handle;
}

/* The pen to set: null, raised (no state, or v outside lo to hi). */
static Pen *pen_for(const int32_t v, const int32_t lo, const int32_t hi)
{
    Gx *const g = gx_get();

    if (g && (v < lo || v > hi))
        gx_fail(ERR_ILLEGAL);

    return g && v >= lo && v <= hi ? &g->pen : nullptr;
}

void gfx_pen_light(const int32_t level)
{
    Pen *const p = pen_for(level, 0, PAL_FULL);

    if (p)
        p->light = level;
}

void gfx_pen_tint(const int32_t colour)
{
    Pen *const p = pen_for(colour, 0, PAL_WHITE);

    if (p)
        p->tint = (uint32_t)colour;
}

void gfx_pen_alpha(const int32_t level)
{
    Pen *const p = pen_for(level, 0, PAL_FULL);

    if (p)
        p->alpha = level;
}

void gfx_pen_recolor(const int32_t row)
{
    Pen *const p = pen_for(row, 0, PAL_ROWS - 1);

    if (p)
        p->recolor = row;
}
