/* gfx.draw's settings: the style every call after draws with, and fonts (draw.bas). */
#include "core/gfx.h"

/* The style to set: null, raised (no state, or v outside lo to hi). */
static Style *style_for(const int32_t v, const int32_t lo, const int32_t hi)
{
    Gx *const g = gx_get();
    return g && !gx_illegal(v < lo || v > hi, "a value past the setting's range") ? &g->style
                                                                                  : nullptr;
}

/* The style to read: null, no state (raised). */
static const Style *style(void)
{
    const Gx *const g = gx_get();
    return g ? &g->style : nullptr;
}

void gfx_draw_set_light(const int32_t level)
{
    Style *const s = style_for(level, 0, PAL_FULL);

    if (s)
        s->light = level;
}

int32_t gfx_draw_get_light(void)
{
    const Style *const s = style();
    return s ? s->light : 0;
}

void gfx_draw_set_tint(const int32_t colour)
{
    Style *const s = style_for(colour, 0, PAL_WHITE);

    if (s)
        s->tint = (uint32_t)colour;
}

int32_t gfx_draw_get_tint(void)
{
    const Style *const s = style();
    return s ? (int32_t)s->tint : 0;
}

void gfx_draw_set_alpha(const int32_t level)
{
    Style *const s = style_for(level, 0, PAL_FULL);

    if (s)
        s->alpha = level;
}

int32_t gfx_draw_get_alpha(void)
{
    const Style *const s = style();
    return s ? s->alpha : 0;
}

void gfx_draw_set_palette_row(const int32_t row)
{
    Style *const s = style_for(row, 0, PAL_ROWS - 1);

    if (s)
        s->palette_row = row;
}

int32_t gfx_draw_get_palette_row(void)
{
    const Style *const s = style();
    return s ? s->palette_row : 0;
}

void gfx_draw_set_font(const int32_t f)
{
    Gx *const g = gx_get();

    if (g && !gx_illegal(!font_get(&g->fonts, (uint32_t)f), "no such font"))
        g->style.font = (uint32_t)f;
}

int32_t gfx_draw_get_font(void)
{
    const Style *const s = style();
    return s ? (int32_t)s->font : 0;
}

void gfx_draw_reset(void)
{
    Gx *const g = gx_get();

    if (g)
        style_reset(&g->style);
}

/* Its cells from the sheet's top left; those past 255 dropped. */
int32_t gfx_draw_new_font(const int32_t sheet, const int32_t w, const int32_t h)
{
    Gx *const g = gx_get();
    const Surface *const s = g ? gx_surface(g, (uint32_t)sheet) : nullptr;

    if (!s)
        return 0;

    if (w < 1 || h < 1 || w > s->w || h > s->h)
        return gx_fail_why(ERR_ILLEGAL, "a glyph below 1 pixel, or larger than the sheet");

    const uint32_t f = font_new(&g->fonts, (uint32_t)sheet, s->w, s->h, w, h);
    return f ? (int32_t)f : gx_fail(ERR_MEMORY);
}

/* The built-in is not new_font's. */
void gfx_draw_free_font(const int32_t f)
{
    Gx *const g = gx_get();

    if (!g || gx_illegal(!f || !font_get(&g->fonts, (uint32_t)f), "no such font, or the built-in"))
        return;

    font_free(&g->fonts, (uint32_t)f);

    if (g->style.font == (uint32_t)f)
        g->style.font = 0;
}
