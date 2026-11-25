/* Polygons (soft.h): flat, affine or in perspective, through the scanner (scanner.h). */
#include "gfx_qx.h"
#include "backend/soft/span.h"

/* gfx's corner as qgl's: qgl's pixel i spans [i, i + 1) and its texel j is centred on j, where
   gfx centres pixel i on i and texel j on j + 0.5. */
static ScanVert qgl_corner(const float x, const float y, const float u, const float v)
{
    return (ScanVert){x + 0.5f, y + 0.5f, 1, u - 0.5f, v - 0.5f};
}

/* an affine span: its walk, u wrapped when it wraps */
static void textured(void *const ctx, const ScanSpan *const sp)
{
    const Textured *const x = ctx;
    const int32_t wrap = x->wrap ? x->s->w << 16 : 0;
    const Steps st = steps_from(sp->u, sp->v, sp->du, sp->dv, wrap);

    span_texels(x, sp->x0, sp->y, sp->x1 - sp->x0, st, sp->z, sp->dz);
}

/* a perspective span: span.h walks it in runs */
static void perspective(void *const ctx, const ScanSpan *const sp) { span_persp(ctx, sp); }

static void texture(
    const SoftTarget *const t,
    const Pixels *const s,
    const ScanVert *const v,
    const uint32_t n,
    const int32_t flags,
    const bool wrap,
    const ScanMode mode
)
{
    const Textured x = {
        .t = t,
        .s = s,
        .opaque = flags & GFX_DRAW_OPAQUE,
        .wrap = wrap,
        .test = flags & GFX_DRAW_DEPTH_TEST,
        .set = flags & GFX_DRAW_DEPTH_WRITE,
        .smooth = flags & GFX_DRAW_SMOOTH,
    };

    if (span_fused(t, s) && !x.smooth && !wrap) {
        const uint32_t o = x.opaque ? FILL_OPAQUE : 0;
        const uint32_t z = t->depth.p ? (x.test ? ROW_TEST : 0) | (x.set ? ROW_WRITE : 0) : 0;
        Rows r = {.dst = t->px, .src = *s, .depth = t->depth};

        if (mode == SCAN_PTEX)
            scan_persp(v, n, t->clip, &r, t->fill->persp[o][z]);
        else
            scan_affine(v, n, t->clip, &r, t->fill->affine[o][z][0], t->fill->affine[o][z][1]);

        return;
    }

    scan_poly(v, n, mode, t->clip, mode == SCAN_PTEX ? perspective : textured, (void *)&x);
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

    if (!t->depth.p) {
        span_fill(t, sp->y, sp->x0, sp->x1, f->colour);
        return;
    }

    uint16_t *const d = t->depth.p + sp->y * t->depth.row;
    int32_t z = sp->z, from = sp->x0;

    for (int32_t x = sp->x0; x < sp->x1; x++, z = depth_on(z, sp->dz, 1)) {
        const uint16_t here = depth_of(z);

        if (f->test && !depth_shows(d[x], here)) {
            span_fill(t, sp->y, from, x, f->colour);
            from = x + 1;
        } else if (f->set)
            d[x] = here;
    }

    span_fill(t, sp->y, from, sp->x1, f->colour);
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
    const bool persp = s && flags & GFX_DRAW_PERSPECTIVE;

    if (n > SCAN_MAXV)
        return;

    for (uint32_t i = 0; i < n; i++)
        v[i] = poly_corner(&c[i], persp);

    if (s)
        texture(t, s, v, n, flags, wrap, persp ? SCAN_PTEX : SCAN_TEX);
    else {
        const Flat f = {t, colour, flags & GFX_DRAW_DEPTH_TEST, flags & GFX_DRAW_DEPTH_WRITE};

        scan_poly(v, n, SCAN_FLAT, t->clip, flat, (void *)&f);
    }
}
