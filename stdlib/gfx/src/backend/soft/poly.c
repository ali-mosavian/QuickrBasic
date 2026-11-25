/* Polygons (soft.h): flat, affine or in perspective, through the scanner (scanner.h). */
#include "gfx_qx.h"
#include "backend/soft/span.h"

/* gfx's corner as qgl's: qgl's pixel i spans [i, i + 1) and its texel j is centred on j, where
   gfx centres pixel i on i and texel j on j + 0.5. */
static ScanVert qgl_corner(const float x, const float y, const float u, const float v)
{
    return (ScanVert){x + 0.5f, y + 0.5f, 1, u - 0.5f, v - 0.5f};
}

/* The flags' depths (ROW_'s), none without a depth. */
static uint32_t depths_of(const SoftTarget *const t, const int32_t flags)
{
    if (!t->depth.p)
        return 0;

    return (flags & GFX_DRAW_DEPTH_TEST ? ROW_TEST : 0) |
           (flags & GFX_DRAW_DEPTH_WRITE ? ROW_WRITE : 0);
}

/* A texture's polygon: its row filler picked once, then scanned. The fused fillers when the ink
   has nothing to do and u does not wrap for a strip, a row leaving a tiled texture (px_tiles)
   wrapped; an affine row inside the texture, no depth, by its inked filler; any other by the
   general one; SMOOTH's by its own. */
static void texture(
    const SoftTarget *const t,
    const Pixels *const s,
    const ScanVert *const v,
    const uint32_t n,
    const int32_t flags,
    const bool wrap,
    const bool persp
)
{
    const Fill *const fill = t->fill;
    const uint32_t o = flags & GFX_DRAW_OPAQUE ? FILL_OPAQUE : 0;
    const uint32_t z = depths_of(t, flags);
    const bool tiled = !(flags & GFX_DRAW_SMOOTH) && px_tiles(s->w, s->h);
    Rows r = {
        .dst = t->px,
        .src = *s,
        .depth = t->depth,
        .ink = &t->ink,
        .depths = z,
        .wrap = wrap || tiled ? s->w << 16 : 0,
        .wrap_v = tiled ? s->h << 16 : 0,
    };

    if (flags & GFX_DRAW_SMOOTH) {
        if (persp)
            scan_persp(v, n, t->clip, &r, fill->smooth_persp);
        else
            scan_affine(v, n, t->clip, &r, fill->smooth, fill->smooth);
    } else if (span_fused(t, s) && !wrap) {
        if (persp)
            scan_persp(v, n, t->clip, &r, fill->persp[o][z]);
        else
            scan_affine(
                v,
                n,
                t->clip,
                &r,
                fill->affine[o][z][ROW_PLAIN],
                fill->affine[o][z][tiled ? ROW_WRAPPED : ROW_HELD]
            );
    } else if (span_fused(t, s) && !persp) {
        RowAffine *const wrapped = fill->affine[o][z][ROW_WRAPPED];

        scan_affine(v, n, t->clip, &r, wrapped, wrapped);
    } else if (persp)
        scan_persp(v, n, t->clip, &r, fill->general_persp);
    else if (!z && !wrap) {
        RowAffine *const inked =
            fill->inked[s->format][fill_variant(&t->ink, t->px.format, s->format)];

        scan_affine(v, n, t->clip, &r, inked, fill->general);
    } else
        scan_affine(v, n, t->clip, &r, fill->general, fill->general);
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
        texture(t, s, v, n, flags, wrap, persp);
    else {
        Rows r = {
            .dst = t->px,
            .depth = t->depth,
            .ink = &t->ink,
            .depths = depths_of(t, flags),
        };

        RowFlat *const fill = t->fill->pick_flat(&r, colour, t->tints);

        scan_flat(v, n, t->clip, &r, fill);
    }
}
