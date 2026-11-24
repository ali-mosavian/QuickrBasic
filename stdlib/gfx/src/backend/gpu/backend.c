/* A GPU as a backend (backend.h): a call's shapes and pictures as triangles in the list
 * (backend/gpu/gpu.h), the style in their vertices.
 *
 * # How it works
 * A shape is SOLID, its colour in the vertices (an index through the style's palette row at 8 bits) times
 * the style's tint, so its texel does not matter and it joins any batch; alone it draws from the
 * white texel. A picture's vertices carry its texels, its rectangle and its
 * stages. Pixels are squares about whole numbers, so a shape's edges lie half a pixel round the
 * centres it covers, as software's. A line covers software's pixels, one a step along its longer
 * side. A circle or an arc is one box, the shader keeping the pixels disc.h's rule covers.
 */
#include "base/disc.h"
#include "core/gfx.h"
#include "base/gmath.h"
#include "core/mem.h"
#include "base/cover.h"
#include "backend/gpu/gpu.h"

enum { ALPHA = 255 };

/* The GPU's own (Gx.own): its recorder and the call readied. */
typedef struct {
    Gpu gpu;
    GpuTarget call;
} GpuState;

enum { QUAD = 4 };

/* n vertices, four a quad. */
static void emit(const GpuTarget *const t, const GpuVertex *const v, const uint32_t n)
{
    if (!gpu_draw(t->gpu, t, v, n))
        gx_fail(ERR_MEMORY);
}

/* A vertex of the call at x, y in the target, the texel u, v in its picture. */
static GpuVertex vertex(
    const GpuTarget *const t,
    const GpuVertex *const ink,
    const float x,
    const float y,
    const float u,
    const float v
)
{
    GpuVertex out = *ink;
    out.x = x + t->at.x;
    out.y = y + t->at.y;
    out.u = u + ink->rx;
    out.v = v + ink->ry;
    return out;
}

/* Corners a, b, c, d round; uv their texels, or null the white texel's. */
static void quad(
    const GpuTarget *const t,
    const GpuVertex *const ink,
    const Vec *const p,
    const Vec *const uv
)
{
    GpuVertex v[QUAD];

    for (uint32_t i = 0; i < QUAD; i++)
        v[i] = vertex(t, ink, p[i].x, p[i].y, uv ? uv[i].x : ink->u, uv ? uv[i].y : ink->v);

    emit(t, v, QUAD);
}

/* A fan's corner i, from ctx. */
typedef GpuVertex CornerAt(const GpuTarget *t, const void *ctx, uint32_t i);

/* The fan from corner 0 of n as quads: two of its triangles each, the last alone a quad with its
   third corner twice, its second triangle empty. */
static void fan(
    const GpuTarget *const t,
    CornerAt *const corner,
    const void *const ctx,
    const uint32_t n
)
{
    for (uint32_t i = 1; i + 1 < n; i += 2) {
        const GpuVertex v[QUAD] = {
            corner(t, ctx, 0),
            corner(t, ctx, i),
            corner(t, ctx, i + 1),
            corner(t, ctx, i + 2 < n ? i + 2 : i + 1),
        };

        emit(t, v, QUAD);
    }
}

/* The box from x0, y0 to x1, y1 in a shape's ink. */
static void box(
    const GpuTarget *const t,
    const GpuVertex *const ink,
    const float x0,
    const float y0,
    const float x1,
    const float y1
)
{
    quad(t, ink, (const Vec[]){{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}, nullptr);
}

/* A shape's ink: its colour, an index through the row at 8 bits, times the tint. */
static GpuVertex shape(const GpuTarget *const t, const uint32_t colour)
{
    const uint32_t rgb = pal_shade(t->eight ? t->row[colour] : colour, PAL_FULL, t->tint);
    return (GpuVertex){
        .u = 0.5f,
        .v = 0.5f,
        .rw = 1,
        .rh = 1,
        .r = (uint8_t)(rgb >> 16),
        .g = (uint8_t)(rgb >> 8),
        .b = (uint8_t)rgb,
        .light = t->ink.light,
        .stages = GPU_STAGE_SOLID,
    };
}

static Gpu *gpu(const Gx *const g) { return &((GpuState *)g->own)->gpu; }

static GpuTarget *call(const Gx *const g) { return &((GpuState *)g->own)->call; }

static bool begin(Gx *const g, const Command *const c, const Picture *const into, const Rect clip)
{
    const Style *const style = &g->style;
    Gpu *const gp = gpu(g);
    const Picture white = {{(uint8_t *)&gp->white, 4, 1, 1, PX_32}, {0, 0, 1, 1}, PIC_WHITE, 1};

    const Rect in_root = {into->at.x + clip.x, into->at.y + clip.y, clip.w, clip.h};

    const QxBlend blend = c->flags & GFX_DRAW_ADD     ? QX_BLEND_ADD
                          : c->flags & GFX_DRAW_BLEND ? QX_BLEND_MIX
                                                      : QX_BLEND_NONE;
    GpuSource target, blank;

    if (!gpu_texture(gp, &g->palette, into, true, &target) ||
        !gpu_texture(gp, &g->palette, &white, false, &blank) ||
        !gpu_target(gp, target.id, into->root.w, into->root.h, in_root) ||
        !gpu_palette(gp, &g->palette))
        return gx_fail(ERR_MEMORY);

    *call(g) = (GpuTarget){
        .gpu = gp,
        .at = {(float)into->at.x, (float)into->at.y},
        .clip = clip,
        .ink =
            {
                .light = (uint8_t)style->light,
                .row = (uint8_t)style->palette_row,
                .stages = (uint8_t)((c->flags & GFX_DRAW_OPAQUE ? 0 : GPU_STAGE_KEYED) |
                                    (c->flags & GFX_DRAW_SMOOTH ? GPU_STAGE_SMOOTH : 0)),
            },
        .texture = blank.id,
        .blend = blend,
        .alpha = blend ? (uint8_t)(style->alpha * ALPHA / PAL_FULL) : ALPHA,
        .eight = into->root.format == PX_8,
        .row = g->palette.rows[style->palette_row],
        .tint = style->tint,
        .solid = c->flags & GFX_DRAW_SOLID,
        .paint = style->tint,
    };

    return true;
}

static bool source(Gx *const g, const Picture *const p)
{
    GpuTarget *const t = call(g);
    GpuSource from;

    if (!gpu_texture(gpu(g), &g->palette, p, false, &from))
        return gx_fail(ERR_MEMORY);

    GpuVertex *const k = &t->ink;
    t->texture = from.id;
    k->rx = (uint16_t)(from.x + p->at.x);
    k->ry = (uint16_t)(from.y + p->at.y);
    k->rw = (uint16_t)p->at.w;
    k->rh = (uint16_t)p->at.h;
    k->r = (uint8_t)(t->paint >> 16);
    k->g = (uint8_t)(t->paint >> 8);
    k->b = (uint8_t)t->paint;
    k->stages =
        (uint8_t)((k->stages & (GPU_STAGE_KEYED | GPU_STAGE_SMOOTH)) |
                  (from.indexed ? GPU_STAGE_INDEXED : 0) | (t->solid ? GPU_STAGE_SOLID : 0));
    return true;
}

static void solid(Gx *const g, const uint32_t colour)
{
    GpuTarget *const t = call(g);
    const GpuVertex ink = shape(t, colour);
    t->solid = true;
    t->paint = (uint32_t)ink.r << 16 | (uint32_t)ink.g << 8 | ink.b;
}

static void clear(Gx *const g, const uint32_t colour)
{
    const GpuTarget *const t = call(g);
    const GpuVertex ink = shape(t, colour);
    const Rect c = t->clip;
    box(t,
        &ink,
        (float)c.x - 0.5f,
        (float)c.y - 0.5f,
        (float)(c.x + c.w) - 0.5f,
        (float)(c.y + c.h) - 0.5f);
}

static void pset(Gx *const g, const Vec p, const uint32_t colour)
{
    const GpuTarget *const t = call(g);
    const GpuVertex ink = shape(t, colour);
    const float x = (float)cover_pixel(p.x), y = (float)cover_pixel(p.y);
    box(t, &ink, x - 0.5f, y - 0.5f, x + 0.5f, y + 0.5f);
}

/* Software's line (Bresenham's) from a's pixel to b's, b's only when last: a parallelogram a pixel
   across the longer side, about the line between the pixels' centres, so each step along that
   side covers the one pixel nearest the line. */
static void segment(
    const GpuTarget *const t,
    const GpuVertex *const ink,
    const Vec a,
    const Vec b,
    const bool last
)
{
    const float x = (float)cover_pixel(a.x), y = (float)cover_pixel(a.y);
    const float dx = (float)cover_pixel(b.x) - x, dy = (float)cover_pixel(b.y) - y;
    const bool wide = (dx < 0 ? -dx : dx) >= (dy < 0 ? -dy : dy);
    const float run = wide ? dx : dy, slope = run != 0 ? (wide ? dy : dx) / run : 0;
    const float half = run < 0 ? -0.5f : 0.5f, from = -half, to = run + (last ? half : -half);
    const Vec p[] = {
        wide ? (Vec){x + from, y + from * slope - 0.5f} : (Vec){x + from * slope - 0.5f, y + from},
        wide ? (Vec){x + to, y + to * slope - 0.5f} : (Vec){x + to * slope - 0.5f, y + to},
        wide ? (Vec){x + to, y + to * slope + 0.5f} : (Vec){x + to * slope + 0.5f, y + to},
        wide ? (Vec){x + from, y + from * slope + 0.5f} : (Vec){x + from * slope + 0.5f, y + from},
    };

    if (from != to)
        quad(t, ink, p, nullptr);
}

static void line(Gx *const g, const Vec a, const Vec b, const uint32_t colour)
{
    const GpuTarget *const t = call(g);
    const GpuVertex ink = shape(t, colour);
    segment(t, &ink, a, b, true);
}

static void outline(Gx *const g, const Vec *const p, const uint32_t n, const uint32_t colour)
{
    const GpuTarget *const t = call(g);
    const GpuVertex ink = shape(t, colour);

    for (uint32_t i = 0; i < n; i++)
        segment(t, &ink, p[i], p[(i + 1) % n], false);
}

/* A polygon's: its ink and corners; flat, the ink's texel. */
typedef struct {
    GpuVertex ink;
    const Corner *c;
    bool flat;
} Shape;

static GpuVertex shape_corner(const GpuTarget *const t, const void *const ctx, const uint32_t i)
{
    const Shape *const s = ctx;
    const Corner *const c = &s->c[i];
    return vertex(t, &s->ink, c->x, c->y, s->flat ? s->ink.u : c->u, s->flat ? s->ink.v : c->v);
}

/* Affine, with no depth: perspective and depth are software's alone so far. */
static void poly(
    Gx *const g,
    const Picture *const p,
    const Corner *const c,
    const uint32_t n,
    const uint32_t colour,
    const int32_t flags
)
{
    const GpuTarget *const t = call(g);
    Shape s = {p ? t->ink : shape(t, colour), c, !p};

    s.ink.stages |= flags & POLY_WRAP ? GPU_STAGE_WRAP : 0;
    fan(t, shape_corner, &s, n);
}

static bool clear_depth(Gx *const g, const Picture *const into)
{
    (void)g, (void)into;
    return true;
}

/* k's pixels, in sweep w unless null: a box past its outer edge, of which the shader keeps those
   k covers (disc.h). Its centre is the root's: in a view, exact when on a whole or half pixel. */
static void disc(
    const GpuTarget *const t,
    const uint32_t colour,
    const Disc *const k,
    const Sweep *const w
)
{
    const float e = k->outer + 1;
    const bool part = w && !w->whole;
    GpuDisc d = {.v = shape(t, colour)};

    if (!(k->outer > 0))
        return;

    d.v.stages = (uint8_t)(GPU_STAGE_SOLID | GPU_STAGE_DISC | (part ? GPU_STAGE_SWEEP : 0) |
                           (part && w->wide ? GPU_STAGE_WIDE : 0));
    d.disc[0] = k->c.x + t->at.x;
    d.disc[1] = k->c.y + t->at.y;
    d.disc[2] = k->outer;
    d.disc[3] = k->inner;

    if (part) {
        d.sweep[0] = w->s.x;
        d.sweep[1] = w->s.y;
        d.sweep[2] = w->f.x;
        d.sweep[3] = w->f.y;
    }

    const Vec p[QUAD] = {
        {k->c.x - e, k->c.y - e},
        {k->c.x + e, k->c.y - e},
        {k->c.x + e, k->c.y + e},
        {k->c.x - e, k->c.y + e},
    };

    GpuDisc v[QUAD];

    for (uint32_t i = 0; i < QUAD; i++) {
        v[i] = d;
        v[i].v = vertex(t, &d.v, p[i].x, p[i].y, d.v.u, d.v.v);
    }

    if (!gpu_discs(t->gpu, t, v, QUAD))
        gx_fail(ERR_MEMORY);
}

static void circle(Gx *const g, const Vec c, const float r, const uint32_t colour, const bool fill)
{
    const Disc k = disc_of(c, r, fill);
    disc(call(g), colour, &k, nullptr);
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
    const Disc k = disc_of(c, r, false);
    const Sweep w = sweep_of(start, finish);
    disc(call(g), colour, &k, &w);
}

static void rect(Gx *const g, const Vec a, const Vec b, const uint32_t colour, const bool fill)
{
    const GpuTarget *const t = call(g);
    const GpuVertex ink = shape(t, colour);
    const float l = (a.x < b.x ? a.x : b.x) - 0.5f, r = (a.x < b.x ? b.x : a.x) + 0.5f;
    const float top = (a.y < b.y ? a.y : b.y) - 0.5f, bottom = (a.y < b.y ? b.y : a.y) + 0.5f;

    if (fill) {
        box(t, &ink, l, top, r, bottom);
        return;
    }

    /* The pixels whose centres lie inside, as software's: their rows and columns at the edges. */
    const float x0 = (float)cover_first(l) - 0.5f, x1 = (float)cover_first(r) - 0.5f;
    const float y0 = (float)cover_first(top) - 0.5f, y1 = (float)cover_first(bottom) - 0.5f;

    if (x0 >= x1 || y0 >= y1)
        return;

    box(t, &ink, x0, y0, x1, y0 + 1);

    if (y1 - 1 > y0)
        box(t, &ink, x0, y1 - 1, x1, y1);

    if (y1 - y0 > 2) {
        box(t, &ink, x0, y0 + 1, x0 + 1, y1 - 1);

        if (x1 - 1 > x0)
            box(t, &ink, x1 - 1, y0 + 1, x1, y1 - 1);
    }
}

static void sprite(Gx *const g, const Picture *const p, const Vec at, const int32_t flags)
{
    const GpuTarget *const t = call(g);
    const float w = (float)p->at.w, h = (float)p->at.h;
    const float x0 = at.x - 0.5f, y0 = at.y - 0.5f;
    const float u0 = flags & GFX_DRAW_FLIP_X ? w : 0, v0 = flags & GFX_DRAW_FLIP_Y ? h : 0;
    const float u1 = w - u0, v1 = h - v0;
    const Vec corners[] = {{x0, y0}, {x0 + w, y0}, {x0 + w, y0 + h}, {x0, y0 + h}};

    const Vec uv[] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};

    quad(t, &t->ink, corners, uv);
}

static bool screen_open(Gx *const g, const QxScreen *const s, const PxFormat f)
{
    const uint32_t pitch = s->w * px_bytes(f);
    uint8_t *const page = mem_alloc(pitch * s->h);
    GpuState *const o = mem_alloc(sizeof *o);

    g->own = o;

    if (!page || !o || !gpu_open(&o->gpu, s) ||
        !surf_open(&g->surfaces, (Pixels){page, pitch, s->w, s->h, f})) {
        if (o)
            gpu_close(&o->gpu);

        mem_free(page);
        mem_free(o);
        g->own = nullptr;
        return gx_fail(ERR_MEMORY);
    }

    o->gpu.page = page;
    return true;
}

static void screen_flip(Gx *const g)
{
    if (!gpu_palette(gpu(g), &g->palette))
        gx_fail(ERR_MEMORY);

    gpu_play(gpu(g), true);
}

static bool sync(Gx *const g, const uint32_t h)
{
    uint32_t root_h = h;
    const Surface *const root = surf_root(&g->surfaces, surf_get(&g->surfaces, h), &root_h);
    const Pixels px = surf_pixels(&g->surfaces, root);
    return gpu_sync(gpu(g), &g->palette, px, root_h) || gx_fail(ERR_MEMORY);
}

static void stop(Gx *const g)
{
    GpuState *const o = g->own;

    if (!o)
        return;

    gpu_close(&o->gpu);
    mem_free(o->gpu.page);
    mem_free(o);
    g->own = nullptr;
}

/* What the GPU drew read back, but the canvas, which goes. */
static void screen_close(Gx *const g)
{
    const Gpu *const gp = gpu(g);

    for (uint32_t slot = 0; slot < gp->ntextures; slot++) {
        const GpuTexture *const e = &gp->textures[slot];

        if (e->drawn && e->handle != SURF_SCREEN && surf_get(&g->surfaces, e->handle))
            sync(g, e->handle);
    }

    stop(g);
}

static BackendCount counted(Gx *const g)
{
    const GpuCount c = gpu_counted(gpu(g));
    return (BackendCount){c.batches, c.uploads, c.waits};
}

static void wrote(Gx *const g, const uint32_t h, const int32_t top, const int32_t bottom)
{
    gpu_wrote(gpu(g), h, top, bottom);
}

static void forget(Gx *const g, const uint32_t h) { gpu_forget(gpu(g), h); }

const Backend backend_gpu = {
    .has = GFX_DISPLAY_HAS_GPU | GFX_DISPLAY_HAS_SMOOTH,
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
