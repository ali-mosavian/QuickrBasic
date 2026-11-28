/* Polygons (soft.h): flat, affine or in perspective, through the scanner (scanner.h). */
#include "gfx_qx.h"
#include "backend/soft/span.h"
#include "core/shape.h"

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

/* The rows of a polygon of s's texels: u repeating when wrap or s tiles (px_tiles), v when it
   tiles. */
static Rows rows_of(
    const SoftTarget *const t,
    const Pixels *const s,
    const int32_t flags,
    const bool wrap
)
{
    const bool tiled = !(flags & GFX_DRAW_SMOOTH) && px_tiles(s->w, s->h);

    return (Rows){
        .dst = t->px,
        .src = *s,
        .depth = t->depth,
        .ink = &t->ink,
        .depths = depths_of(t, flags),
        .wrap = wrap || tiled ? s->w << 16 : 0,
        .wrap_v = tiled ? s->h << 16 : 0,
    };
}

/* Of n levels, the one an affine polygon reads: from its first three corners' texels' steps,
   constant over it. 0 when they are in a line. */
static uint32_t affine_level(const ScanVert *const v, const uint32_t n)
{
    const float ax = v[1].x - v[0].x, ay = v[1].y - v[0].y;
    const float bx = v[2].x - v[0].x, by = v[2].y - v[0].y;
    const float d = ax * by - bx * ay;

    if (!(d < 0 || d > 0))
        return 0;

    const float au = v[1].u - v[0].u, bu = v[2].u - v[0].u;
    const float av = v[1].v - v[0].v, bv = v[2].v - v[0].v;

    return scan_level(
        (au * by - bu * ay) / d,
        (av * by - bv * ay) / d,
        (bu * ax - au * bx) / d,
        (bv * ax - av * bx) / d,
        n
    );
}

/* A texture's polygon: its row filler picked once, then scanned. The fused fillers when the ink
   has nothing to do and u does not wrap for a strip, a row leaving a tiled texture (px_tiles)
   wrapped; an affine row inside the texture, no depth, by its inked filler; any other by the
   general one; SMOOTH's by its own. With levels (MIPMAP) an affine polygon reads one level, its
   corners' texels scaled to it, and a perspective one a level a row (scan_persp). */
static void texture(
    const SoftTarget *const t,
    const Pixels *const texels,
    const Levels *const levels,
    const ScanVert *const corners,
    const uint32_t n,
    const int32_t flags,
    const bool wrap,
    const bool persp
)
{
    const Fill *const fill = t->fill;
    const uint32_t o = flags & GFX_DRAW_OPAQUE ? FILL_OPAQUE : 0;
    const uint32_t z = depths_of(t, flags);
    const bool mipmap = levels && levels->n > 1;
    const uint32_t k = mipmap && !persp ? affine_level(corners, levels->n) : 0;
    const Pixels *const s = k ? &levels->at[k] : texels;
    const bool tiled = !(flags & GFX_DRAW_SMOOTH) && px_tiles(s->w, s->h);
    ScanVert scaled[SCAN_MAXV];
    const ScanVert *v = corners;
    Rows r = rows_of(t, s, flags, wrap);
    ScanLevels lv;
    ScanLevels *const each = mipmap && persp ? &lv : nullptr;

    if (k) {
        const float su = (float)s->w / (float)texels->w, sv = (float)s->h / (float)texels->h;

        for (uint32_t i = 0; i < n; i++) {
            scaled[i] = corners[i];
            scaled[i].u = su * corners[i].u + (su - 1) / 2;
            scaled[i].v = sv * corners[i].v + (sv - 1) / 2;
        }

        v = scaled;
    }

    if (each) {
        lv.n = levels->n;

        for (uint32_t i = 0; i < lv.n; i++) {
            const Pixels *const at = i ? &levels->at[i] : texels;

            lv.rows[i] = rows_of(t, at, flags, wrap);
            lv.su[i] = (float)at->w / (float)texels->w;
            lv.sv[i] = (float)at->h / (float)texels->h;
        }
    }

    if (flags & GFX_DRAW_SMOOTH) {
        if (persp)
            scan_persp(v, n, t->clip, &r, fill->smooth_persp, each);
        else
            scan_affine(v, n, t->clip, &r, fill->smooth, fill->smooth);
    } else if (span_fused(t, s) && !wrap) {
        if (persp)
            scan_persp(v, n, t->clip, &r, fill->persp[o][z], each);
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
        scan_persp(v, n, t->clip, &r, fill->general_persp, each);
    else if (!z && !wrap) {
        RowAffine *const inked =
            fill->inked[s->format][fill_variant(&t->ink, t->px.format, s->format)];

        scan_affine(v, n, t->clip, &r, inked, fill->general);
    } else
        scan_affine(v, n, t->clip, &r, fill->general, fill->general);
}

/* c as qgl's vertex (qgl_corner), 1/z its z; in perspective u and v by 1/z. A 2D corner's z is 1,
   its 1/z no divide. */
static ScanVert poly_corner(const Corner *const c, const bool persp)
{
    const float r = c->z == 1 ? 1 : 1 / c->z;
    const ScanVert q = qgl_corner(c->x, c->y, c->u, c->v);

    return persp ? (ScanVert){q.x, q.y, r, q.u * r, q.v * r} : (ScanVert){q.x, q.y, r, q.u, q.v};
}

void soft_poly(
    const SoftTarget *const t,
    const Pixels *const s,
    const Levels *const levels,
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
        texture(t, s, levels, v, n, flags, wrap, persp);
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

void soft_turned(
    const SoftTarget *const t,
    const Pixels *const s,
    const Turn *const turn,
    const int32_t flags
)
{
    if (!(flags & GFX_DRAW_SMOOTH) && !depths_of(t, flags) && span_fused(t, s) && s->w < 32768 &&
        s->h < 32768) {
        const uint32_t o = flags & GFX_DRAW_OPAQUE ? FILL_OPAQUE : 0;
        const bool tiled = px_tiles(s->w, s->h);
        Rows r = {
            .dst = t->px,
            .src = *s,
            .depth = t->depth,
            .ink = &t->ink,
            .wrap = tiled ? s->w << 16 : 0,
            .wrap_v = tiled ? s->h << 16 : 0,
        };
        if (scan_turned(
                turn->at.x,
                turn->at.y,
                turn->across.x,
                turn->across.y,
                turn->down.x,
                turn->down.y,
                s->w,
                s->h,
                t->clip,
                &r,
                t->fill->turned[o]
            ))
            return;
    }

    Corner v[4];
    turn_corners(turn, s->w, s->h, v);
    soft_poly(t, s, nullptr, v, 4, 0, flags, false);
}
