/* Software as a backend (backend.h): the pen made an ink (fill.h), and soft.h's drawing. */
#include "gfx.h"

/* The ink of a call's flags and the pen over a target of format, its texels a shape's colours:
   false, raised. */
static bool ink(Gx *const g, const int32_t flags, const PxFormat format, Ink *const k)
{
    const Pen *const p = &g->pen;
    Palette *const pal = &g->palette;
    const bool eight = format == PX_8;
    const BlendMode mode = flags & GFX_PEN_ADD     ? BLEND_ADD
                           : flags & GFX_PEN_BLEND ? BLEND_MIX
                                                   : BLEND_NONE;
    const uint32_t solid = pal_shade(p->tint, p->light, PAL_WHITE);
    uint8_t index = 0;
    *k = (Ink){
        .source = eight ? PX_8 : PX_32,
        .keyed = !(flags & GFX_PEN_OPAQUE),
        .solid = flags & GFX_PEN_SOLID,
        .colour = PX_OPAQUE | solid,
        .shade = pal_shade(PAL_WHITE, p->light, p->tint),
        .mode = eight ? BLEND_NONE : mode,
        .alpha = (uint8_t)p->alpha,
    };

    if (eight && !pal_lut(pal, p->recolor, p->light, p->tint, &k->lut))
        return gx_fail(ERR_MEMORY);

    if (eight && k->solid && !pal_index(pal, solid, &index))
        return gx_fail(ERR_MEMORY);

    if (eight && mode && !(k->blend = pal_blend(pal, mode, p->alpha)))
        return gx_fail(ERR_MEMORY);

    k->colour = eight ? index : k->colour;
    k->effects = k->solid || mode || (eight ? k->lut != nullptr : k->shade != PAL_WHITE);
    return true;
}

static bool begin(Gx *const g, Surface *const s, const Command *const c, Target *const t)
{
    const Pixels px = surf_pixels(&g->surfaces, s);
    t->soft = (SoftTarget){
        .px = px,
        .clip = t->clip,
        .fill = fill_formats[px.format],
    };

    return ink(g, c->flags, px.format, &t->soft.ink);
}

static bool source(Gx *const g, Target *const t, const Picture *const p)
{
    Ink *const k = &t->soft.ink;
    const Pen *const pen = &g->pen;
    const PxFormat from = p->root.format;
    const bool eight = t->soft.px.format == PX_8;
    k->source = from;

    if (eight && from != PX_8 && !k->map && !(k->map = pal_map(&g->palette)))
        return gx_fail(ERR_MEMORY);

    if (!eight && from == PX_8 && !k->argb &&
        !(k->argb = pal_argb(&g->palette, pen->recolor, pen->light, pen->tint)))
        return gx_fail(ERR_MEMORY);

    return true;
}

static void solid(const Gx *const g, Target *const t, const uint32_t colour)
{
    Ink *const k = &t->soft.ink;
    const Pen *const p = &g->pen;
    const bool eight = t->soft.px.format == PX_8;
    k->solid = k->effects = true;
    k->colour = eight ? (k->lut ? k->lut[colour] : colour)
                      : PX_OPAQUE | pal_shade(colour, p->light, p->tint);
}

static void clear(const Target *const t, const uint32_t colour) { soft_clear(&t->soft, colour); }

static void pset(const Target *const t, const Vec p, const uint32_t colour)
{
    soft_pset(&t->soft, p.x, p.y, colour);
}

static void line(const Target *const t, const Vec a, const Vec b, const uint32_t colour)
{
    soft_line(&t->soft, a, b, colour);
}

static void outline(
    const Target *const t,
    const Vec *const p,
    const uint32_t n,
    const uint32_t colour
)
{
    soft_outline(&t->soft, p, n, colour);
}

static void poly(const Target *const t, const Vec *const p, const uint32_t n, const uint32_t colour)
{
    soft_poly(&t->soft, p, n, colour);
}

static void circle(
    const Target *const t,
    const Vec c,
    const float r,
    const uint32_t colour,
    const bool fill
)
{
    soft_circle(&t->soft, c, r, colour, fill);
}

static void arc(
    const Target *const t,
    const Vec c,
    const float r,
    const float start,
    const float finish,
    const uint32_t colour
)
{
    soft_arc(&t->soft, c, r, start, finish, colour);
}

static void rect(
    const Target *const t,
    const Vec a,
    const Vec b,
    const uint32_t colour,
    const bool fill
)
{
    soft_rect(&t->soft, a.x, a.y, b.x, b.y, colour, fill);
}

static void sprite(const Target *const t, const Picture *const p, const Vec at, const int32_t flags)
{
    const Pixels px = pic_pixels(p);
    soft_sprite(&t->soft, &px, at.x, at.y, flags);
}

static void texture(
    const Target *const t,
    const Picture *const p,
    const Vertex *const v,
    const uint32_t n,
    const int32_t flags,
    const bool wrap
)
{
    const Pixels px = pic_pixels(p);
    soft_texture(&t->soft, &px, v, n, flags, wrap);
}

const Backend backend_soft = {
    .begin = begin,
    .source = source,
    .solid = solid,
    .clear = clear,
    .pset = pset,
    .line = line,
    .outline = outline,
    .poly = poly,
    .circle = circle,
    .arc = arc,
    .rect = rect,
    .sprite = sprite,
    .texture = texture,
};
