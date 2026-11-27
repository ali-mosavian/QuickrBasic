/* A GPU as a backend (backend.h): a call's shapes and textures as triangles in the list
 * (backend/gpu/gpu.h), the style in their vertices.
 *
 * # How it works
 * A shape is SOLID, its colour in the vertices (an index through the style's palette row at 8 bits) lit
 * and tinted here as software's ink, so its texel does not matter and it joins any batch; alone it
 * draws from the white texel. SOLID's colour is worked here likewise. A texture's vertices carry its texels, its rectangle and its
 * stages. Pixels are squares about whole numbers, so a shape's edges lie half a pixel round the
 * centres it covers, as software's. A line covers software's pixels, one a step along its longer
 * side. A circle or an arc is one box, the shader keeping the pixels disc.h's rule covers. A
 * polygon's corners carry their depth, 1/z, and, in perspective, their z as w, so the GPU steps
 * texels and depth as software's scanner does; clear_depth draws depth 0 and no colour. On a
 * fixed host (fixed.h) an 8-bit target's blends are the host's and its texels nearest, and a
 * circle or an arc is its rows.
 */
#include "base/disc.h"
#include "core/gfx.h"
#include "base/gmath.h"
#include "core/mem.h"
#include "base/cover.h"
#include "backend/gpu/gpu.h"

/* ALPHA: a pipeline's whole alpha. CALL_STAGES: the stages a call's flags set, kept for its
   textures. */
enum {
    ALPHA = 255,
    SHAPE_STAGES =
        GPU_STAGE_ADD | GPU_STAGE_MIX | GPU_STAGE_EIGHT | GPU_STAGE_FIFTEEN | GPU_STAGE_SIXTEEN,
    CALL_STAGES = GPU_STAGE_KEYED | GPU_STAGE_SMOOTH | SHAPE_STAGES,
};

/* The textures an aim found, kept while the target's texture and version and the GPU's epoch hold:
   each draw found them again, 30k cycles of MEADOW's 486 frame. */
typedef struct {
    bool held;
    uint32_t epoch, texture, version;
    GpuSource target, blank;
} Aimed;

/* The GPU's own (Gx.own): its recorder, the call readied and the textures last aimed at. */
typedef struct {
    Gpu gpu;
    GpuCall call;
    Aimed aimed;
} GpuState;

enum { QUAD = 4 };

/* n vertices, four a quad. */
static void emit(const GpuCall *const t, const GpuVertex *const v, const uint32_t n)
{
    if (!gpu_draw(t->gpu, t, v, n))
        gx_fail(ERR_MEMORY);
}

/* A vertex of the call at x, y in the target, the texel u, v in its texture. */
static GpuVertex vertex(
    const GpuCall *const t,
    const GpuVertex *const template,
    const float x,
    const float y,
    const float u,
    const float v
)
{
    GpuVertex out = *template;
    out.x = x + t->at.x;
    out.y = y + t->at.y;
    out.u = u + template->rx;
    out.v = v + template->ry;
    return out;
}

/* Corners a, b, c, d round; uv their texels, or null the white texel's. */
static void emit_quad(
    const GpuCall *const t,
    const GpuVertex *const template,
    const Vec *const p,
    const Vec *const uv
)
{
    GpuVertex v[QUAD];

    for (uint32_t i = 0; i < QUAD; i++)
        v[i] = vertex(
            t,
            template,
            p[i].x,
            p[i].y,
            uv ? uv[i].x : template->u,
            uv ? uv[i].y : template->v
        );

    emit(t, v, QUAD);
}

/* A fan's corner i, from ctx. */
typedef GpuVertex CornerAt(const GpuCall *t, const void *ctx, uint32_t i);

/* The fan from corner 0 of n as quads: two of its triangles each, the last alone a quad with its
   third corner twice, its second triangle empty. */
static void fan(
    const GpuCall *const t,
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

/* The box from x0, y0 to x1, y1 in a shape's template. */
static void box(
    const GpuCall *const t,
    const GpuVertex *const template,
    const float x0,
    const float y0,
    const float x1,
    const float y1
)
{
    emit_quad(t, template, (const Vec[]){{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}, nullptr);
}

/* An index as a vertex's r, g and b: itself in each. */
static uint32_t spread(const uint32_t index) { return (index & 0xFF) * 0x010101u; }

/* A shape's template: its colour lit and tinted as software's ink draws a texel of it; at 8 bits, an
   index through the lut. */
static GpuVertex shape(const GpuCall *const t, const uint32_t colour)
{
    const uint32_t light = t->template.light, index = colour & 0xFF;
    const uint32_t rgb = t->eight ? spread(t->lut ? t->lut[index] : index)
                                  : px_shade(colour, pal_shade(PAL_WHITE, (int32_t)light, t->tint));
    return (GpuVertex){
        .w = 1,
        .u = 0.5f,
        .v = 0.5f,
        .rw = 1,
        .rh = 1,
        .r = (uint8_t)(rgb >> 16),
        .g = (uint8_t)(rgb >> 8),
        .b = (uint8_t)rgb,
        .light = (uint8_t)light,
        .alpha = t->template.alpha,
        .stages = (uint16_t)(GPU_STAGE_SOLID | (t->template.stages & SHAPE_STAGES)),
    };
}

static Gpu *gpu(const Gx *const g) { return &((GpuState *)g->own)->gpu; }

static GpuCall *call(const Gx *const g) { return &((GpuState *)g->own)->call; }

/* The pass into into within clip, with its depth texture when deep (made if none, *fresh then),
   blank, the white texel, and the palette, its map too when map: false, no memory. */
static bool aim(
    Gx *const g,
    const Source *const into,
    const Rect clip,
    const bool deep,
    const bool map,
    bool *const fresh,
    GpuSource *const blank
)
{
    Gpu *const gp = gpu(g);
    Aimed *const k = &((GpuState *)g->own)->aimed;
    const Rect in_root = {into->at.x + clip.x, into->at.y + clip.y, clip.w, clip.h};

    const bool kept = k->held && k->epoch == gp->epoch && k->texture == into->texture &&
                      k->version == into->version;
    const Source white = {{(uint8_t *)&gp->white, 4, 1, 1, PX_32}, {0, 0, 1, 1}, SOURCE_WHITE, 1};

    k->held = kept;

    if (!(kept || gpu_texture(gp, into, true, false, &k->target)) ||
        (deep && !gpu_depth(gp, into, &k->target.depth, fresh)) ||
        !(kept || gpu_texture(gp, &white, false, false, &k->blank)))
        return false;

    k->held = true;
    k->epoch = gp->epoch;
    k->texture = into->texture;
    k->version = into->version;
    *blank = k->blank;
    return gpu_target(gp, k->target.id, k->target.depth, into->root.w, into->root.h, in_root) &&
           gpu_palette(gp, &g->palette, map);
}

static bool begin(Gx *const g, const Command *const c, const Source *const into, const Rect clip)
{
    const Style *const style = &g->style;
    const PxFormat f = into->root.format;
    /* an 8-bit target's colours are indices: the shader blends and matches them itself, through
       the map; a fixed host's draws its texels nearest, a mix of indices none */
    const bool eight = f == PX_8, shaded = eight && !gpu(g)->fixed;
    const bool smooth = c->flags & GFX_DRAW_SMOOTH && (!eight || shaded);
    const bool add = c->flags & GFX_DRAW_ADD, mix = !add && c->flags & GFX_DRAW_BLEND;
    /* the pipeline's blend is not narrowed after */
    const bool kept = !add && !mix;
    const uint8_t depth = (c->flags & GFX_DRAW_DEPTH_TEST ? QX_DEPTH_TEST : 0) |
                          (c->flags & GFX_DRAW_DEPTH_WRITE ? QX_DEPTH_WRITE : 0);
    /* read by a SOLID draw's texels alone (source) */
    const uint32_t paint =
        c->flags & GFX_DRAW_SOLID ? pal_shade(style->tint, style->light, PAL_WHITE) : 0;
    const uint8_t *lut = nullptr;
    uint8_t index = 0;
    GpuSource blank;
    bool fresh;

    /* at 8 bits: the lut, SOLID's index, and the map for a blend or a lut */
    if ((eight && !pal_lut(&g->palette, style->palette_row, style->light, style->tint, &lut)) ||
        (eight && c->flags & GFX_DRAW_SOLID && !pal_index(&g->palette, paint, &index)) ||
        !aim(g, into, clip, depth, shaded && (add || mix || lut), &fresh, &blank))
        return gx_fail(ERR_MEMORY);

    *call(g) = (GpuCall){
        .gpu = gpu(g),
        .at = {(float)into->at.x, (float)into->at.y},
        .clip = clip,
        .template =
            {
                .w = 1,
                .light = (uint8_t)style->light,
                .row = (uint8_t)style->palette_row,
                .alpha = (uint8_t)style->alpha,
                .stages = (uint16_t)((c->flags & GFX_DRAW_OPAQUE ? 0 : GPU_STAGE_KEYED) |
                                     (smooth ? GPU_STAGE_SMOOTH : 0) | (add ? GPU_STAGE_ADD : 0) |
                                     (mix && shaded ? GPU_STAGE_MIX : 0) |
                                     (shaded ? GPU_STAGE_EIGHT : 0) |
                                     (kept && f == PX_15 ? GPU_STAGE_FIFTEEN : 0) |
                                     (kept && f == PX_16 ? GPU_STAGE_SIXTEEN : 0)),
            },
        .texture = blank.id,
        .blend = shaded ? QX_BLEND_NONE
                 : add  ? QX_BLEND_ADD
                 : mix  ? QX_BLEND_MIX
                        : QX_BLEND_NONE,
        .depth = depth,
        .perspective = c->flags & GFX_DRAW_PERSPECTIVE,
        /* the shader takes ADD's share itself; a fixed host, the draw's alpha */
        .alpha = !shaded && (mix || (add && gpu(g)->fixed))
                     ? (uint8_t)((style->alpha * ALPHA + PAL_FULL / 2) / PAL_FULL)
                     : ALPHA,
        .eight = eight,
        .lut = lut,
        .tint = style->tint,
        .solid = c->flags & GFX_DRAW_SOLID,
        .paint = eight ? spread(index) : paint,
        .behind = shaded && (mix || add),
    };

    return true;
}

static bool source(Gx *const g, const Source *const p)
{
    GpuCall *const t = call(g);
    GpuVertex *const k = &t->template;
    GpuSource from;

    /* at 8 bits, a colour drawn matched by the map: on a fixed host, matched as it is sent */
    if (!gpu_texture(gpu(g), p, false, t->eight && !t->solid, &from) ||
        (t->eight && (!from.indexed || k->stages & GPU_STAGE_SMOOTH) &&
         !gpu_palette(gpu(g), &g->palette, true)))
        return gx_fail(ERR_MEMORY);

    const uint32_t rgb = t->solid ? t->paint : t->tint;
    t->texture = from.id;
    k->rx = (uint16_t)(from.x + p->at.x);
    k->ry = (uint16_t)(from.y + p->at.y);
    k->rw = (uint16_t)p->at.w;
    k->rh = (uint16_t)p->at.h;
    k->r = (uint8_t)(rgb >> 16);
    k->g = (uint8_t)(rgb >> 8);
    k->b = (uint8_t)rgb;
    k->stages = (uint16_t)((k->stages & CALL_STAGES) | (from.indexed ? GPU_STAGE_INDEXED : 0) |
                           (from.alpha ? GPU_STAGE_ALPHA : 0) | (t->solid ? GPU_STAGE_SOLID : 0));
    return true;
}

/* Every texel drawn is colour lit and tinted, as ink_solid's: at 8 bits, through the lut. */
static void solid(Gx *const g, const uint32_t colour)
{
    GpuCall *const t = call(g);
    const uint32_t index = colour & 0xFF;
    t->solid = true;
    t->paint = t->eight ? spread(t->lut ? t->lut[index] : index)
                        : pal_shade(colour, t->template.light, t->tint);
}

static void clear(Gx *const g, const uint32_t colour)
{
    const GpuCall *const t = call(g);
    const GpuVertex template = shape(t, colour);
    const Rect c = t->clip;
    box(t,
        &template,
        (float)c.x - 0.5f,
        (float)c.y - 0.5f,
        (float)(c.x + c.w) - 0.5f,
        (float)(c.y + c.h) - 0.5f);
}

static void pset(Gx *const g, const Vec p, const uint32_t colour)
{
    const GpuCall *const t = call(g);
    const GpuVertex template = shape(t, colour);
    const float x = (float)cover_pixel(p.x), y = (float)cover_pixel(p.y);
    box(t, &template, x - 0.5f, y - 0.5f, x + 0.5f, y + 0.5f);
}

/* Software's line (Bresenham's) from a's pixel to b's, b's only when last: a parallelogram a pixel
   across the longer side, about the line between the pixels' centres, so each step along that
   side covers the one pixel nearest the line. */
static void segment(
    const GpuCall *const t,
    const GpuVertex *const template,
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
        emit_quad(t, template, p, nullptr);
}

static void line(Gx *const g, const Vec a, const Vec b, const uint32_t colour)
{
    const GpuCall *const t = call(g);
    const GpuVertex template = shape(t, colour);
    segment(t, &template, a, b, true);
}

static void outline(Gx *const g, const Vec *const p, const uint32_t n, const uint32_t colour)
{
    const GpuCall *const t = call(g);
    const GpuVertex template = shape(t, colour);

    for (uint32_t i = 0; i < n; i++)
        segment(t, &template, p[i], p[(i + 1) % n], false);
}

/* A polygon's: its template and corners; flat, the template's texel; perspective, its texels by 1/z. */
typedef struct {
    GpuVertex template;
    const Corner *c;
    bool flat, perspective;
} Shape;

static GpuVertex shape_corner(const GpuCall *const t, const void *const ctx, const uint32_t i)
{
    const Shape *const s = ctx;
    const Corner *const c = &s->c[i];
    GpuVertex v = vertex(
        t,
        &s->template,
        c->x,
        c->y,
        s->flat ? s->template.u : c->u,
        s->flat ? s->template.v : c->v
    );

    v.z = gm_reciprocal(c->z);
    v.w = s->perspective ? c->z : 1;
    return v;
}

static void poly(
    Gx *const g,
    const Source *const p,
    const Corner *const c,
    const uint32_t n,
    const uint32_t colour,
    const int32_t flags
)
{
    const GpuCall *const t = call(g);
    Shape s = {p ? t->template : shape(t, colour), c, !p, p && t->perspective};

    const bool tiled = p && !(s.template.stages & GPU_STAGE_SMOOTH) && px_tiles(p->at.w, p->at.h);

    s.template.stages |= flags & POLY_WRAP ? GPU_STAGE_WRAP : 0;
    s.template.stages |= tiled ? GPU_STAGE_TILE : 0;
    fan(t, shape_corner, &s, n);
}

/* A box over into writing depth 0 and no colour; none when its depth texture is new, all 0. */
static bool clear_depth(Gx *const g, const Source *const into)
{
    const Rect all = {0, 0, into->at.w, into->at.h};

    GpuSource blank;
    bool fresh;

    if (!aim(g, into, all, true, false, &fresh, &blank))
        return gx_fail(ERR_MEMORY);

    if (fresh)
        return true;

    const GpuCall t = {
        .gpu = gpu(g),
        .at = {(float)into->at.x, (float)into->at.y},
        .clip = all,
        .template =
            {
                .w = 1,
                .u = 0.5f,
                .v = 0.5f,
                .rw = 1,
                .rh = 1,
                .stages = GPU_STAGE_SOLID,
            },
        .texture = blank.id,
        .blend = QX_BLEND_KEEP,
        .depth = QX_DEPTH_WRITE,
        .alpha = ALPHA,
    };

    box(&t, &t.template, -0.5f, -0.5f, (float)all.w - 0.5f, (float)all.h - 0.5f);
    return true;
}

/* A disc's rows drawn: the call and their template. */
typedef struct {
    const GpuCall *t;
    GpuVertex template;
} Rows;

static void row(void *const ctx, const int32_t y, const int32_t x0, const int32_t x1)
{
    const Rows *const r = ctx;
    box(r->t, &r->template, (float)x0 - 0.5f, (float)y - 0.5f, (float)x1 - 0.5f, (float)y + 0.5f);
}

/* k's pixels, in sweep w unless null: a box past its outer edge, of which the shader keeps those
   k covers (disc.h); on a fixed host, its rows' runs. Its centre is the root's: in a view, exact
   when on a whole or half pixel. */
static void disc(
    const GpuCall *const t,
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

    if (t->gpu->fixed) {
        Rows rows = {t, d.v};

        disc_runs(k, part ? w : nullptr, t->clip.y, t->clip.y + t->clip.h, row, &rows);
        return;
    }

    d.v.stages |= (uint16_t)(GPU_STAGE_DISC | (part ? GPU_STAGE_SWEEP : 0) |
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
    const GpuCall *const t = call(g);
    const GpuVertex template = shape(t, colour);
    const float l = (a.x < b.x ? a.x : b.x) - 0.5f, r = (a.x < b.x ? b.x : a.x) + 0.5f;
    const float top = (a.y < b.y ? a.y : b.y) - 0.5f, bottom = (a.y < b.y ? b.y : a.y) + 0.5f;

    if (fill) {
        box(t, &template, l, top, r, bottom);
        return;
    }

    /* The pixels whose centres lie inside, as software's: their rows and columns at the edges. */
    const float x0 = (float)cover_first(l) - 0.5f, x1 = (float)cover_first(r) - 0.5f;
    const float y0 = (float)cover_first(top) - 0.5f, y1 = (float)cover_first(bottom) - 0.5f;

    if (x0 >= x1 || y0 >= y1)
        return;

    box(t, &template, x0, y0, x1, y0 + 1);

    if (y1 - 1 > y0)
        box(t, &template, x0, y1 - 1, x1, y1);

    if (y1 - y0 > 2) {
        box(t, &template, x0, y0 + 1, x0 + 1, y1 - 1);

        if (x1 - 1 > x0)
            box(t, &template, x1 - 1, y0 + 1, x1, y1 - 1);
    }
}

static void sprite(Gx *const g, const Source *const p, const Vec at, const int32_t flags)
{
    const GpuCall *const t = call(g);
    const float w = (float)p->at.w, h = (float)p->at.h;
    const float x0 = at.x - 0.5f, y0 = at.y - 0.5f;
    const float u0 = flags & GFX_DRAW_FLIP_X ? w : 0, v0 = flags & GFX_DRAW_FLIP_Y ? h : 0;
    const float u1 = w - u0, v1 = h - v0;
    const Vec corners[] = {{x0, y0}, {x0 + w, y0}, {x0 + w, y0 + h}, {x0, y0 + h}};

    const Vec uv[] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};

    emit_quad(t, &t->template, corners, uv);
}

static bool screen_open(Gx *const g, const QxScreen *const s, const PxFormat f)
{
    const uint32_t pitch = s->w * px_bytes(f);
    uint8_t *const page = mem_alloc(pitch * s->h);
    GpuState *const o = mem_alloc(sizeof *o);

    g->own = o;

    if (!page || !o || !gpu_open(&o->gpu, s, f == PX_8, &g->palette) ||
        !surf_open(&g->surfaces, (Pixels){page, pitch, s->w, s->h, f})) {
        if (o)
            gpu_close(&o->gpu);

        mem_free(page);
        mem_free(o);
        g->own = nullptr;
        return gx_fail(ERR_MEMORY);
    }

    o->gpu.page = page;
    o->aimed = (Aimed){};
    return true;
}

static void screen_flip(Gx *const g)
{
    if (!gpu_palette(gpu(g), &g->palette, false))
        gx_fail(ERR_MEMORY);

    gpu_play(gpu(g), true);
}

static bool sync(Gx *const g, const uint32_t h)
{
    uint32_t root_h = h;
    const Surface *const root = surf_root(&g->surfaces, surf_get(&g->surfaces, h), &root_h);
    const Pixels px = surf_pixels(&g->surfaces, root);
    return gpu_sync(gpu(g), px, root_h) || gx_fail(ERR_MEMORY);
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

/* A fan of c's corners: the two triangles. */
static void quad(Gx *const g, const Source *const p, const Corner c[4], const int32_t flags)
{
    poly(g, p, c, 4, 0, flags);
}

static void turned(Gx *const g, const Source *const p, const Turn *const t, const int32_t flags)
{
    Corner v[4];
    turn_corners(t, p->at.w, p->at.h, v);
    poly(g, p, v, 4, 0, flags);
}

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
    .turned = turned,
    .quad = quad,
};
