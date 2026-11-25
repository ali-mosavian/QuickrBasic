/* Software as a backend (backend.h): the style made an ink (ink.h), and soft.h's drawing. Its own
   state holds the call readied and each root's depth. */
#include "core/gfx.h"
#include "core/mem.h"
#include "backend/soft/soft.h"

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
    FillTints tints;
    FillMixes mixes;
} Soft;

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
    Pixels canvas = g->surfaces.screen;
    canvas.p = qx_host->gfx_page(&canvas.pitch);
    surf_page(&g->surfaces, canvas);
}

/* The canvas's depth goes with it: the next is of another size. */
static void screen_close(Gx *const g)
{
    if (g->own)
        drop_depth(g->own, SURF_SCREEN);
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
        .tints = &o->tints,
    };

    if (c->flags & (GFX_DRAW_DEPTH_TEST | GFX_DRAW_DEPTH_WRITE) &&
        !(o->call.depth = depth(o, into)).p)
        return false;

    return ink_begin(&o->call.ink, &g->palette, &g->style, c->flags, px.format) ||
           gx_fail(ERR_MEMORY);
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
    SoftTarget *const t = &o->call;

    if (!ink_source(&t->ink, &g->palette, &g->style, t->px.format, p->root.format))
        return gx_fail(ERR_MEMORY);

    if (t->px.format != PX_8 && t->ink.mode && !t->ink.solid)
        t->ink.mixes = fill_mixes(&o->mixes, t->px.format, &t->ink);

    return true;
}

static void solid(Gx *const g, const uint32_t colour)
{
    SoftTarget *const t = &((Soft *)g->own)->call;
    ink_solid(&t->ink, &g->style, t->px.format, colour);
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
    .has = GFX_DISPLAY_HAS_SMOOTH,
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
