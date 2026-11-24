/* gfx.pen's entries: where drawing goes and the numbers its flags use (pen.bas). */
#include "core/gfx.h"

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

    if (gx_illegal(w < 0 || h < 0))
        return;

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

/* Its cells from the sheet's top left; those past 255 dropped. */
int32_t gfx_pen_new_font(const int32_t sheet, const int32_t w, const int32_t h)
{
    Gx *const g = gx_get();
    const Surface *const s = g ? gx_surface(g, (uint32_t)sheet) : nullptr;

    if (!s)
        return 0;

    if (w < 1 || h < 1 || w > s->w || h > s->h)
        return gx_fail(ERR_ILLEGAL);

    const uint32_t f = font_new(&g->fonts, (uint32_t)sheet, s->w, s->h, w, h);
    return f ? (int32_t)f : gx_fail(ERR_MEMORY);
}

void gfx_pen_font(const int32_t handle)
{
    Gx *const g = gx_get();

    if (!g)
        return;

    if (gx_illegal(!font_get(&g->fonts, (uint32_t)handle)))
        return;

    g->pen.font = (uint32_t)handle;
}

/* The pen to set: null, raised (no state, or v outside lo to hi). */
static Pen *pen_for(const int32_t v, const int32_t lo, const int32_t hi)
{
    Gx *const g = gx_get();
    return g && !gx_illegal(v < lo || v > hi) ? &g->pen : nullptr;
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

void gfx_pen_recolour(const int32_t row)
{
    Pen *const p = pen_for(row, 0, PAL_ROWS - 1);

    if (p)
        p->recolour = row;
}
