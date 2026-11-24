/* Software as a backend (backend.h): the pen made an ink (fill.h), and soft.h's drawing. Its own
   state holds the call readied and each root's depth. */
#include "core/gfx.h"
#include "core/mem.h"
#include "soft/soft.h"

/* A root's depth, w by h as the root: 1/z by DEPTH_NEAR, 0 far. */
typedef struct {
    uint32_t handle;
    uint16_t *p;
} SoftDepth;

/* Software's own (Gx.own): the call readied, and the roots' depths by their slots. */
typedef struct {
    SoftTarget call;
    SoftDepth *depths;
    uint32_t ndepths;
} Soft;

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

    if (eight && !pal_lut(pal, p->recolour, p->light, p->tint, &k->lut))
        return gx_fail(ERR_MEMORY);

    if (eight && k->solid && !pal_index(pal, solid, &index))
        return gx_fail(ERR_MEMORY);

    if (eight && mode && !(k->blend = pal_blend(pal, mode, p->alpha)))
        return gx_fail(ERR_MEMORY);

    k->colour = eight ? index : k->colour;
    k->effects = k->solid || mode || (eight ? k->lut != nullptr : k->shade != PAL_WHITE);
    return true;
}

/* Software's own, made on first ask: null, raised. */
static Soft *own(Gx *const g)
{
    if (!g->own && !(g->own = mem_alloc(sizeof(Soft))))
        gx_fail(ERR_MEMORY);

    return g->own;
}

/* The call begin readied. */
static const SoftTarget *call(const Gx *const g) { return &((const Soft *)g->own)->call; }

/* h's depth dropped, h a root. */
static void drop_depth(Soft *const o, const uint32_t h)
{
    const uint32_t slot = hdl_slot(h);

    if (slot < o->ndepths && o->depths[slot].handle == h) {
        mem_free(o->depths[slot].p);
        o->depths[slot] = (SoftDepth){};
    }
}

/* p's depth, its root's made all far on first ask: p null, raised. */
static Depth depth(Soft *const o, const Picture *const p)
{
    const uint32_t slot = hdl_slot(p->texture), row = (uint32_t)p->root.w;

    if (!mem_grow((void **)&o->depths, &o->ndepths, slot + 1, sizeof *o->depths))
        return gx_fail(ERR_MEMORY), (Depth){};

    SoftDepth *const d = &o->depths[slot];

    if (d->handle != p->texture || !d->p) {
        mem_free(d->p);
        *d = (SoftDepth){p->texture, mem_alloc(row * (uint32_t)p->root.h * sizeof *d->p)};
    }

    if (!d->p)
        return gx_fail(ERR_MEMORY), (Depth){};

    return (Depth){d->p + p->at.y * row + p->at.x, row};
}

static bool screen_open(Gx *const g, const QxScreen *const s, const PxFormat f)
{
    uint32_t pitch;
    uint8_t *const page = qx_host->gfx_page(&pitch);

    return surf_open(&g->surfaces, (Pixels){page, pitch, s->w, s->h, f}) || gx_fail(ERR_MEMORY);
}

/* The page shown; the next one lent, the last frame's pixels in it. */
static void screen_flip(Gx *const g)
{
    qx_host->gfx_show(g->palette.rows[0]);
    Pixels canvas = g->surfaces.canvas;
    canvas.p = qx_host->gfx_page(&canvas.pitch);
    surf_page(&g->surfaces, canvas);
}

/* The canvas's depth goes with it: the next is of another size. */
static void screen_close(Gx *const g)
{
    if (g->own)
        drop_depth(g->own, GFX_SURFACE_CANVAS);
}

static void stop(Gx *const g)
{
    Soft *const o = g->own;

    if (!o)
        return;

    for (uint32_t i = 0; i < o->ndepths; i++)
        mem_free(o->depths[i].p);

    mem_free(o->depths);
    mem_free(o);
    g->own = nullptr;
}

static BackendCount counted(Gx *const g)
{
    (void)g;
    return (BackendCount){};
}

static bool sync(Gx *const g, const uint32_t h)
{
    (void)g, (void)h;
    return true;
}

static void wrote(Gx *const g, const uint32_t h, const int32_t top, const int32_t bottom)
{
    (void)g, (void)h, (void)top, (void)bottom;
}

static void forget(Gx *const g, const uint32_t h)
{
    if (g->own)
        drop_depth(g->own, h);
}

static bool begin(Gx *const g, const Command *const c, const Picture *const into, const Rect clip)
{
    Soft *const o = own(g);

    if (!o)
        return false;

    const Pixels px = pic_pixels(into);
    gx_wrote(g, c->target, (Rect){0, 0, px.w, px.h});

    o->call = (SoftTarget){
        .px = px,
        .clip = clip,
        .fill = fill_formats[px.format],
    };

    if (c->flags & (GFX_DRAW_ZTEST | GFX_DRAW_ZSET) && !(o->call.depth = depth(o, into)).p)
        return false;

    return ink(g, c->flags, px.format, &o->call.ink);
}

static bool clear_depth(Gx *const g, const Picture *const into)
{
    Soft *const o = own(g);
    const Depth d = o ? depth(o, into) : (Depth){};

    if (!d.p)
        return false;

    for (int32_t y = 0; y < into->at.h; y++)
        mem_fill(d.p + y * d.row, 0, (size_t)into->at.w * sizeof *d.p);

    return true;
}

static bool source(Gx *const g, const Picture *const p)
{
    Soft *const o = g->own;
    Ink *const k = &o->call.ink;
    const Pen *const pen = &g->pen;
    const PxFormat from = p->root.format;
    const bool eight = o->call.px.format == PX_8;
    k->source = from;

    if (eight && from != PX_8 && !k->map && !(k->map = pal_map(&g->palette)))
        return gx_fail(ERR_MEMORY);

    if (!eight && from == PX_8 && !k->argb &&
        !(k->argb = pal_argb(&g->palette, pen->recolour, pen->light, pen->tint)))
        return gx_fail(ERR_MEMORY);

    return true;
}

static void solid(Gx *const g, const uint32_t colour)
{
    Soft *const o = g->own;
    Ink *const k = &o->call.ink;
    const Pen *const p = &g->pen;
    const bool eight = o->call.px.format == PX_8;
    k->solid = k->effects = true;
    k->colour = eight ? (k->lut ? k->lut[colour] : colour)
                      : PX_OPAQUE | pal_shade(colour, p->light, p->tint);
}

static void clear(Gx *const g, const uint32_t colour) { soft_clear(call(g), colour); }

static void pset(Gx *const g, const Vec p, const uint32_t colour)
{
    soft_pset(call(g), p.x, p.y, colour);
}

static void line(Gx *const g, const Vec a, const Vec b, const uint32_t colour)
{
    soft_line(call(g), a, b, colour);
}

static void outline(Gx *const g, const Vec *const p, const uint32_t n, const uint32_t colour)
{
    soft_outline(call(g), p, n, colour);
}

static void poly(
    Gx *const g,
    const Picture *const p,
    const Corner *const c,
    const uint32_t n,
    const uint32_t colour,
    const int32_t flags
)
{
    const Pixels px = p ? pic_pixels(p) : (Pixels){};

    soft_poly(call(g), p ? &px : nullptr, c, n, colour, flags & ~POLY_WRAP, flags & POLY_WRAP);
}

static void circle(Gx *const g, const Vec c, const float r, const uint32_t colour, const bool fill)
{
    soft_circle(call(g), c, r, colour, fill);
}

static void arc(
    Gx *const g,
    const Vec c,
    const float r,
    const float start,
    const float finish,
    const uint32_t colour
)
{
    soft_arc(call(g), c, r, start, finish, colour);
}

static void rect(Gx *const g, const Vec a, const Vec b, const uint32_t colour, const bool fill)
{
    soft_rect(call(g), a.x, a.y, b.x, b.y, colour, fill);
}

static void sprite(Gx *const g, const Picture *const p, const Vec at, const int32_t flags)
{
    const Pixels px = pic_pixels(p);
    soft_sprite(call(g), &px, at.x, at.y, flags);
}

const Backend backend_soft = {
    .open = screen_open,
    .flip = screen_flip,
    .close = screen_close,
    .stop = stop,
    .counted = counted,
    .sync = sync,
    .wrote = wrote,
    .forget = forget,
    .begin = begin,
    .source = source,
    .solid = solid,
    .clear = clear,
    .pset = pset,
    .line = line,
    .outline = outline,
    .poly = poly,
    .clear_depth = clear_depth,
    .circle = circle,
    .arc = arc,
    .rect = rect,
    .sprite = sprite,
};
