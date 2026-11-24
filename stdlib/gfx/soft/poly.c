/* Polygons (soft.h): flat, affine or in perspective, through the scanner (scanner.h). */
#include "gfx_qx.h"
#include "soft/span.h"

/* gfx's corner as qgl's: qgl's pixel i spans [i, i + 1) and its texel j is centred on j, where
   gfx centres pixel i on i and texel j on j + 0.5. */
static ScanVert qgl_corner(const float x, const float y, const float u, const float v)
{
    return (ScanVert){x + 0.5f, y + 0.5f, 1, u - 0.5f, v - 0.5f};
}

/* A span of the polygon, held in the clip: its first pixel, and how many it lost before it. */
static bool held_span(
    const SoftTarget *const t,
    const ScanSpan *const sp,
    int32_t *const x0,
    int32_t *const x1
)
{
    *x0 = rect_max(sp->x0, t->clip.x);
    *x1 = rect_min(sp->x1, t->clip.x + t->clip.w);

    return sp->y >= t->clip.y && sp->y < t->clip.y + t->clip.h && *x0 < *x1;
}

/* an affine span: its walk, u wrapped when it wraps */
static void textured(void *const ctx, const ScanSpan *const sp)
{
    const Textured *const x = ctx;
    int32_t x0, x1;

    if (!held_span(x->t, sp, &x0, &x1))
        return;

    const int32_t lost = x0 - sp->x0, wrap = x->wrap ? x->s->w << 16 : 0;
    const Steps st = steps_skip(steps_from(sp->u, sp->v, sp->du, sp->dv, wrap), lost);

    span_texels(x, x0, sp->y, x1 - x0, st, depth_on(sp->z, sp->dz, lost), sp->dz);
}

/* a perspective span: span.h walks it in runs */
static void perspective(void *const ctx, const ScanSpan *const sp)
{
    const Textured *const x = ctx;
    ScanSpan c = *sp;
    int32_t x0, x1;

    if (!held_span(x->t, sp, &x0, &x1))
        return;

    const double lost = x0 - sp->x0;
    c.x0 = x0;
    c.x1 = x1;
    c.pu += sp->dpu * lost;
    c.pv += sp->dpv * lost;
    c.pz += sp->dpz * lost;
    c.z = depth_on(sp->z, sp->dz, x0 - sp->x0);
    span_persp(x, &c);
}

static void texture(
    const SoftTarget *const t,
    const Pixels *const s,
    const ScanVert *const v,
    const uint32_t n,
    const int32_t flags,
    const bool wrap,
    const ScanMode mode,
    const bool gfx
)
{
    const Textured x = {
        .t = t,
        .s = s,
        .opaque = flags & GFX_PEN_OPAQUE,
        .wrap = wrap,
        .test = flags & GFX_DRAW_ZTEST,
        .set = flags & GFX_DRAW_ZSET,
    };

    scan_poly(v, n, mode, t->clip, gfx, mode == SCAN_PTEX ? perspective : textured, (void *)&x);
}

void soft_texture_persp(
    const SoftTarget *const t,
    const Pixels *const s,
    const ScanVert *const v,
    const uint32_t n,
    const int32_t flags,
    const bool wrap
)
{
    texture(t, s, v, n, flags, wrap, SCAN_PTEX, false);
}

typedef struct {
    const SoftTarget *t;
    uint32_t colour;
    bool test, set;
} Flat;

/* a flat span: the runs of it shown over the depth there, their depth set as asked */
static void flat(void *const ctx, const ScanSpan *const sp)
{
    const Flat *const f = ctx;
    const SoftTarget *const t = f->t;
    int32_t x0, x1;

    if (!t->depth.p) {
        span_flat(t, sp->y, sp->x0, sp->x1, f->colour);
        return;
    }

    if (!held_span(t, sp, &x0, &x1))
        return;

    uint16_t *const d = t->depth.p + sp->y * t->depth.row;
    int32_t z = depth_on(sp->z, sp->dz, x0 - sp->x0), from = x0;

    for (int32_t x = x0; x < x1; x++, z = depth_on(z, sp->dz, 1)) {
        const uint16_t here = depth_of(z);

        if (f->test && !depth_shows(d[x], here)) {
            span_flat(t, sp->y, from, x, f->colour);
            from = x + 1;
        } else if (f->set)
            d[x] = here;
    }

    span_flat(t, sp->y, from, x1, f->colour);
}

/* c as qgl's vertex (qgl_corner), 1/z its z; in perspective u and v by 1/z. */
static ScanVert poly_corner(const Corner *const c, const bool persp)
{
    const float r = 1 / c->z;
    const ScanVert q = qgl_corner(c->x, c->y, c->u, c->v);

    return persp ? (ScanVert){q.x, q.y, r, q.u * r, q.v * r} : (ScanVert){q.x, q.y, r, q.u, q.v};
}

void soft_poly(
    const SoftTarget *const t,
    const Pixels *const s,
    const Corner *const c,
    const uint32_t n,
    const uint32_t colour,
    const int32_t flags,
    const bool wrap
)
{
    ScanVert v[SCAN_MAXV];
    const bool persp = s && flags & GFX_DRAW_PERSP;

    if (n > SCAN_MAXV)
        return;

    for (uint32_t i = 0; i < n; i++)
        v[i] = poly_corner(&c[i], persp);

    if (s)
        texture(t, s, v, n, flags, wrap, persp ? SCAN_PTEX : SCAN_TEX, true);
    else {
        const Flat f = {t, colour, flags & GFX_DRAW_ZTEST, flags & GFX_DRAW_ZSET};

        scan_poly(v, n, SCAN_FLAT, t->clip, true, flat, (void *)&f);
    }
}
