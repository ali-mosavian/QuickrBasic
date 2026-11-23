/* A GPU as a backend (backend.h): a call's shapes and pictures as triangles in the list
 * (gpu/gpu.h), the pen in their vertices.
 *
 * # How it works
 * A shape is SOLID, its colour in the vertices (an index through the pen's row at 8 bits) times
 * the pen's tint, so its texel does not matter and it joins any batch; alone it draws from the
 * white texel. A picture's vertices carry its texels, its rectangle and its
 * stages. Pixels are squares about whole numbers, so a shape's edges lie half a pixel round the
 * centres it covers, as software's. A line covers software's pixels, one a step along its longer
 * side. A circle or an arc is one box, the shader keeping the pixels disc.h's rule covers.
 */
#include "disc.h"
#include "gfx.h"
#include "gmath.h"
#include "mem.h"
#include "soft/scan.h"

enum { ALPHA = 255 };

enum { QUAD = 4 };

/* n vertices, four a quad. */
static void emit(const Target *const t, const GpuVertex *const v, const uint32_t n)
{
    if (!gpu_draw(t->gpu.gpu, &t->gpu, v, n))
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
    const Target *const t,
    const GpuVertex *const ink,
    const Vec *const p,
    const Vec *const uv
)
{
    const GpuTarget *const g = &t->gpu;
    GpuVertex v[QUAD];

    for (uint32_t i = 0; i < QUAD; i++)
        v[i] = vertex(g, ink, p[i].x, p[i].y, uv ? uv[i].x : ink->u, uv ? uv[i].y : ink->v);

    emit(t, v, QUAD);
}

/* A fan's corner i, from ctx. */
typedef GpuVertex Corner(const Target *t, const void *ctx, uint32_t i);

/* The fan from corner 0 of n as quads: two of its triangles each, the last alone a quad with its
   third corner twice, its second triangle empty. */
static void fan(
    const Target *const t,
    Corner *const corner,
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
    const Target *const t,
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

static bool begin(Gx *const g, Surface *const s, const Command *const c, Target *const t)
{
    const Pen *const pen = &g->pen;
    const Picture p = surf_picture(&g->surfaces, pen->target);
    const Picture white = {{(uint8_t *)&g->gpu.white, 4, 1, 1, PX_32}, {0, 0, 1, 1}, PIC_WHITE, 1};

    const Rect clip = {p.at.x + t->clip.x, p.at.y + t->clip.y, t->clip.w, t->clip.h};

    const QxBlend blend = c->flags & GFX_PEN_ADD     ? QX_BLEND_ADD
                          : c->flags & GFX_PEN_BLEND ? QX_BLEND_MIX
                                                     : QX_BLEND_NONE;
    uint32_t h = pen->target, id, white_id;
    bool indexed;

    if (!gpu_texture(&g->gpu, &g->palette, &p, true, &id, &indexed) ||
        !gpu_texture(&g->gpu, &g->palette, &white, false, &white_id, &indexed) ||
        !gpu_target(&g->gpu, id, p.root.w, p.root.h, clip) || !gpu_palette(&g->gpu, &g->palette))
        return gx_fail(ERR_MEMORY);

    surf_root(&g->surfaces, s, &h)->drawn = true;
    t->gpu = (GpuTarget){
        .gpu = &g->gpu,
        .at = {(float)p.at.x, (float)p.at.y},
        .ink =
            {
                .light = (uint8_t)pen->light,
                .row = (uint8_t)pen->recolor,
                .stages = c->flags & GFX_PEN_OPAQUE ? 0 : GPU_STAGE_KEYED,
            },
        .texture = white_id,
        .blend = blend,
        .alpha = blend ? (uint8_t)(pen->alpha * ALPHA / PAL_FULL) : ALPHA,
        .eight = s->format == PX_8,
        .row = g->palette.rows[pen->recolor],
        .tint = pen->tint,
        .solid = c->flags & GFX_PEN_SOLID,
        .paint = pen->tint,
    };

    return true;
}

static bool source(Gx *const g, Target *const t, const Picture *const p)
{
    GpuTarget *const gt = &t->gpu;
    bool indexed;

    if (!gpu_texture(&g->gpu, &g->palette, p, false, &gt->texture, &indexed))
        return gx_fail(ERR_MEMORY);

    GpuVertex *const k = &gt->ink;
    k->rx = (uint16_t)p->at.x;
    k->ry = (uint16_t)p->at.y;
    k->rw = (uint16_t)p->at.w;
    k->rh = (uint16_t)p->at.h;
    k->r = (uint8_t)(gt->paint >> 16);
    k->g = (uint8_t)(gt->paint >> 8);
    k->b = (uint8_t)gt->paint;
    k->stages = (uint8_t)((k->stages & GPU_STAGE_KEYED) | (indexed ? GPU_STAGE_INDEXED : 0) |
                          (gt->solid ? GPU_STAGE_SOLID : 0));
    return true;
}

static void solid(const Gx *const g, Target *const t, const uint32_t colour)
{
    GpuTarget *const gt = &t->gpu;
    const GpuVertex ink = shape(gt, colour);
    (void)g;
    gt->solid = true;
    gt->paint = (uint32_t)ink.r << 16 | (uint32_t)ink.g << 8 | ink.b;
}

static void clear(const Target *const t, const uint32_t colour)
{
    const GpuVertex ink = shape(&t->gpu, colour);
    const Rect c = t->clip;
    box(t,
        &ink,
        (float)c.x - 0.5f,
        (float)c.y - 0.5f,
        (float)(c.x + c.w) - 0.5f,
        (float)(c.y + c.h) - 0.5f);
}

static void pset(const Target *const t, const Vec p, const uint32_t colour)
{
    const GpuVertex ink = shape(&t->gpu, colour);
    const float x = (float)scan_pixel(p.x), y = (float)scan_pixel(p.y);
    box(t, &ink, x - 0.5f, y - 0.5f, x + 0.5f, y + 0.5f);
}

/* Software's line (Bresenham's) from a's pixel to b's, b's only when last: a parallelogram a pixel
   across the longer side, about the line between the pixels' centres, so each step along that
   side covers the one pixel nearest the line. */
static void segment(
    const Target *const t,
    const GpuVertex *const ink,
    const Vec a,
    const Vec b,
    const bool last
)
{
    const float x = (float)scan_pixel(a.x), y = (float)scan_pixel(a.y);
    const float dx = (float)scan_pixel(b.x) - x, dy = (float)scan_pixel(b.y) - y;
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

static void line(const Target *const t, const Vec a, const Vec b, const uint32_t colour)
{
    const GpuVertex ink = shape(&t->gpu, colour);
    segment(t, &ink, a, b, true);
}

static void outline(
    const Target *const t,
    const Vec *const p,
    const uint32_t n,
    const uint32_t colour
)
{
    const GpuVertex ink = shape(&t->gpu, colour);

    for (uint32_t i = 0; i < n; i++)
        segment(t, &ink, p[i], p[(i + 1) % n], false);
}

/* A polygon's: its ink and points. */
typedef struct {
    GpuVertex ink;
    const Vec *p;
} Shape;

static GpuVertex shape_corner(const Target *const t, const void *const ctx, const uint32_t i)
{
    const Shape *const s = ctx;
    return vertex(&t->gpu, &s->ink, s->p[i].x, s->p[i].y, s->ink.u, s->ink.v);
}

static void poly(const Target *const t, const Vec *const p, const uint32_t n, const uint32_t colour)
{
    const Shape s = {shape(&t->gpu, colour), p};

    fan(t, shape_corner, &s, n);
}

/* k's pixels, in sweep w unless null: a box past its outer edge, of which the shader keeps those
   k covers (disc.h). Its centre is the root's: in a view, exact when on a whole or half pixel. */
static void disc(
    const Target *const t,
    const uint32_t colour,
    const Disc *const k,
    const Sweep *const w
)
{
    const GpuTarget *const g = &t->gpu;
    const float e = k->outer + 1;
    const bool part = w && !w->whole;
    GpuDisc d = {.v = shape(g, colour)};

    if (!(k->outer > 0))
        return;

    d.v.stages = (uint8_t)(GPU_STAGE_SOLID | GPU_STAGE_DISC | (part ? GPU_STAGE_SWEEP : 0) |
                           (part && w->wide ? GPU_STAGE_WIDE : 0));
    d.disc[0] = k->c.x + g->at.x;
    d.disc[1] = k->c.y + g->at.y;
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
        v[i].v = vertex(g, &d.v, p[i].x, p[i].y, d.v.u, d.v.v);
    }

    if (!gpu_discs(g->gpu, g, v, QUAD))
        gx_fail(ERR_MEMORY);
}

static void circle(
    const Target *const t,
    const Vec c,
    const float r,
    const uint32_t colour,
    const bool fill
)
{
    const Disc k = disc_of(c, r, fill);
    disc(t, colour, &k, nullptr);
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
    const Disc k = disc_of(c, r, false);
    const Sweep w = sweep_of(start, finish);
    disc(t, colour, &k, &w);
}

static void rect(
    const Target *const t,
    const Vec a,
    const Vec b,
    const uint32_t colour,
    const bool fill
)
{
    const GpuVertex ink = shape(&t->gpu, colour);
    const float l = (a.x < b.x ? a.x : b.x) - 0.5f, r = (a.x < b.x ? b.x : a.x) + 0.5f;
    const float top = (a.y < b.y ? a.y : b.y) - 0.5f, bottom = (a.y < b.y ? b.y : a.y) + 0.5f;

    if (fill) {
        box(t, &ink, l, top, r, bottom);
        return;
    }

    /* The pixels whose centres lie inside, as software's: their rows and columns at the edges. */
    const float x0 = (float)scan_first(l) - 0.5f, x1 = (float)scan_first(r) - 0.5f;
    const float y0 = (float)scan_first(top) - 0.5f, y1 = (float)scan_first(bottom) - 0.5f;

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

static void sprite(const Target *const t, const Picture *const p, const Vec at, const int32_t flags)
{
    const float w = (float)p->at.w, h = (float)p->at.h;
    const float x0 = at.x - 0.5f, y0 = at.y - 0.5f;
    const float u0 = flags & GFX_DRAW_FLIPX ? w : 0, v0 = flags & GFX_DRAW_FLIPY ? h : 0;
    const float u1 = w - u0, v1 = h - v0;
    const Vec corners[] = {{x0, y0}, {x0 + w, y0}, {x0 + w, y0 + h}, {x0, y0 + h}};

    const Vec uv[] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};

    quad(t, &t->gpu.ink, corners, uv);
}

/* A textured polygon's: its ink and vertices. */
typedef struct {
    GpuVertex ink;
    const Vertex *v;
} Textured;

static GpuVertex textured_corner(const Target *const t, const void *const ctx, const uint32_t i)
{
    const Textured *const s = ctx;
    return vertex(&t->gpu, &s->ink, s->v[i].x, s->v[i].y, s->v[i].u, s->v[i].v);
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
    Textured s = {t->gpu.ink, v};

    (void)p, (void)flags;
    s.ink.stages |= wrap ? GPU_STAGE_WRAP : 0;
    fan(t, textured_corner, &s, n);
}

static bool screen_open(Gx *const g, const QxScreen *const s, const PxFormat f)
{
    const uint32_t pitch = s->w * px_bytes(f);
    uint8_t *const page = mem_alloc(pitch * s->h);

    if (!page || !gpu_open(&g->gpu, s) ||
        !surf_open(&g->surfaces, (Pixels){page, pitch, s->w, s->h, f})) {
        mem_free(page);
        gpu_close(&g->gpu);
        return gx_fail(ERR_MEMORY);
    }

    g->gpu.page = page;
    return true;
}

static void screen_flip(Gx *const g)
{
    if (!gpu_palette(&g->gpu, &g->palette))
        gx_fail(ERR_MEMORY);

    gpu_play(&g->gpu, true);
}

static bool sync(Gx *const g, const uint32_t h)
{
    uint32_t root_h = h;
    Surface *const root = surf_root(&g->surfaces, surf_get(&g->surfaces, h), &root_h);
    const Pixels px = surf_pixels(&g->surfaces, root);
    return gpu_sync(&g->gpu, &g->palette, root, px, root_h) || gx_fail(ERR_MEMORY);
}

/* What the GPU drew read back, but the canvas, which goes. */
static void screen_close(Gx *const g)
{
    for (uint32_t slot = 1; slot < g->surfaces.t.n; slot++) {
        const uint32_t h = hdl_at(&g->surfaces.t, slot);
        const Surface *const s = h ? surf_get(&g->surfaces, h) : nullptr;

        if (s && s->own && s->drawn)
            sync(g, h);
    }

    gpu_close(&g->gpu);
    mem_free(g->gpu.page);
    g->gpu.page = nullptr;
}

static void forget(Gx *const g, const uint32_t h) { gpu_forget(&g->gpu, h); }

const Backend backend_gpu = {
    .open = screen_open,
    .flip = screen_flip,
    .close = screen_close,
    .sync = sync,
    .forget = forget,
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
