/* qgl's convex scanner (scanner.h): rs.asm's qglRsPoly, qgl$Grad, qgl$F2fxA and P, qgl$drawA and P,
   and clip.asm's qglClPolyEx. */
#include "soft/fill.h"
#include "soft/scanner.h"

/* the walk's vertex, qgl's QVertFx: x and y 16.16; u and v 16.16 (affine) or floats */
typedef struct {
    int32_t kx, ky;
    float kz;
    int32_t ku, kv;
    float fu, fv;
} Fx;

/* qgl$Grad's answers: u and v's gradients 16.16 (affine), a pixel's floats (perspective) */
typedef struct {
    double denom;
    int32_t dudx, dvdx;
    float fdudx, fdvdx, fdzdx;
    int32_t dzdx; /* depth's step a pixel, 16.16: qgl$zdzdx */
    int32_t bias; /* gfx's nudge up, 16.16: taken off a prestep, an edge is met at its row */
} Grad;

static inline int32_t add32(const int32_t a, const int32_t b)
{
    return (int32_t)((uint32_t)a + (uint32_t)b);
}

/* FXFLOOR: shr, so a negative is a huge one */
static inline int32_t FXFLOOR(const int32_t a) { return (int32_t)((uint32_t)a >> 16); }

/* a*b >> 16, both 16.16 */
static inline int32_t FXMUL(const int32_t a, const int32_t b)
{
    return (int32_t)((int64_t)a * b >> 16);
}

static inline int32_t FXMUL14(const int32_t a, const int32_t b)
{
    return (int32_t)((int64_t)a * b >> 14);
}

/* a/b, both 16.16 */
static inline int32_t FXDIV(const int32_t a, const int32_t b)
{
    return (int32_t)(((int64_t)a << 16) / b);
}

/* the rows between two 16.16 y, as qgl's word subtract */
static inline int16_t rows_between(const int32_t from, const int32_t to)
{
    return (int16_t)(FXFLOOR(to) - FXFLOOR(from));
}

/* x as qgl stores it to a real4: clang leaves an i386 float in an x87 register, unrounded */
static inline float real4(const double x)
{
    const volatile float f = (float)x;
    return f;
}

static inline bool negative(const float f)
{
    int32_t bits;
    __builtin_memcpy(&bits, &f, sizeof bits);
    return bits < 0;
}

/* Sutherland-Hodgman against one boundary: coordinate (0 x, 1 y) less bound, times sgn, is the
   distance; in is a sign clear. A crossing is lerped and its coordinate snapped to the bound. */
static uint32_t clip_pass(
    const ScanVert *const in,
    const uint32_t n,
    ScanVert *const out,
    const int32_t coord,
    const float bound,
    const float sgn
)
{
    if (n < 3 || n > SCAN_MAXV + 4)
        return 0;

    uint32_t nout = 0;
    const ScanVert *prev = &in[n - 1];
    float dprev = real4(((double)(coord ? prev->y : prev->x) - bound) * sgn);
    bool pin = !negative(dprev);

    for (uint32_t i = 0; i < n; i++) {
        const ScanVert *const cur = &in[i];
        const float dcur = real4(((double)(coord ? cur->y : cur->x) - bound) * sgn);
        const bool cin = !negative(dcur);

        if (cin != pin) {
            const double t = (double)dprev / ((double)dprev - dcur);
            ScanVert o;
            o.x = real4(((double)cur->x - prev->x) * t + prev->x);
            o.y = real4(((double)cur->y - prev->y) * t + prev->y);
            o.z = real4(((double)cur->z - prev->z) * t + prev->z);
            o.u = real4(((double)cur->u - prev->u) * t + prev->u);
            o.v = real4(((double)cur->v - prev->v) * t + prev->v);

            if (coord)
                o.y = bound;
            else
                o.x = bound;

            out[nout++] = o;

            if (nout >= SCAN_MAXV + 4)
                return nout;
        }

        if (cin) {
            out[nout++] = *cur;

            if (nout >= SCAN_MAXV + 4)
                return nout;
        }

        dprev = dcur;
        pin = cin;
        prev = cur;
    }

    return nout;
}

/* qglClPolyEx: the ring in a, four passes (left, right, top, bottom) between a and b. The
   survivors' count, in the buffer returned; 0: none. */
static uint32_t clip_poly(
    ScanVert *const a,
    ScanVert *const b,
    uint32_t n,
    const Rect clip,
    ScanVert **const out
)
{
    const float bound[4] = {
        (float)clip.x,
        (float)(clip.x + clip.w),
        (float)clip.y,
        (float)(clip.y + clip.h),
    };

    ScanVert *from = a, *to = b;

    for (uint32_t pass = 0; pass < 4; pass++) {
        n = clip_pass(from, n, to, pass >> 1, bound[pass], pass & 1 ? -1.0f : 1.0f);

        if (!n)
            return 0;

        ScanVert *const swap = from;
        from = to;
        to = swap;
    }

    *out = from;
    return n;
}

/* (a.f - b.f)*(b.y - c.y) - (b.f - c.f)*(a.y - b.y), qgl's CALC_NOM */
static double nom(
    const float af,
    const float bf,
    const float cf,
    const float ay,
    const float by,
    const float cy
)
{
    return ((double)af - bf) * ((double)by - cy) - ((double)bf - cf) * ((double)ay - by);
}

/* qgl$Grad: false when the triple is degenerate or a gradient will not store. u and v are
   scaled by the texture's size, 1 here. */
static bool grad(
    const ScanVert *const a,
    const ScanVert *const b,
    const ScanVert *const c,
    const bool persp,
    Grad *const g
)
{
    const double r65536 = 0.0000152587890625;
    const double twhole = 1, thwhole = 1;
    const double denom = nom(a->x, b->x, c->x, a->y, b->y, c->y);

    if (!(denom < 0 || denom > 0))
        return false;

    const double rdenom = 65536.0 / denom;
    const double gz16 = nom(a->z, b->z, c->z, a->y, b->y, c->y) * rdenom;
    const double most = 2147483647.0;
    const double zstep = gz16 * DEPTH_NEAR;
    g->fdzdx = real4(gz16 * r65536);

    /* qgl$zscale is DEPTH_NEAR: the depth's step, gated as qgl's in both modes */
    if ((zstep < 0 ? -zstep : zstep) >= most)
        return false;

    g->dzdx = scan_fistp(zstep);

    const double gu16 = nom(a->u, b->u, c->u, a->y, b->y, c->y) * twhole * rdenom;
    g->fdudx = real4(gu16 * r65536);

    const double gv16 = nom(a->v, b->v, c->v, a->y, b->y, c->y) * thwhole * rdenom;
    g->fdvdx = real4(gv16 * r65536);

    const double fu = g->fdudx, fv = g->fdvdx;

    if (persp) {
        if ((fu < 0 ? -fu : fu) >= most || (fv < 0 ? -fv : fv) >= most)
            return false;
    } else {
        if ((gu16 < 0 ? -gu16 : gu16) >= most || (gv16 < 0 ? -gv16 : gv16) >= most)
            return false;

        g->dudx = scan_fistp(gu16);
        g->dvdx = scan_fistp(gv16);
    }

    g->denom = denom;
    return true;
}

/* qgl$F2fxA: half a pixel on x and y, u and v 16.16 */
static Fx to_fixed_a(const ScanVert *const v)
{
    const double half = 0.5, twhole = 1, thwhole = 1;
    Fx f = {};

    f.kx = scan_fistp(((double)v->x + half) * 65536.0);
    f.ky = scan_fistp(((double)v->y + half) * 65536.0);
    f.kz = v->z;
    f.ku = scan_fistp(twhole * v->u * 65536.0);
    f.kv = scan_fistp(thwhole * v->v * 65536.0);
    return f;
}

/* qgl$F2fxP: no half pixel, u and v floats */
static Fx to_fixed_p(const ScanVert *const v)
{
    const double twhole = 1, thwhole = 1;
    Fx f = {};

    f.kx = scan_fistp((double)v->x * 65536.0);
    f.ky = scan_fistp((double)v->y * 65536.0);
    f.kz = v->z;
    f.fu = real4(twhole * v->u);
    f.fv = real4(thwhole * v->v);
    return f;
}

/* An edge's x step a row from its top vertex a to b, over the 16.16 height h: its reciprocal,
   taken with 14 bits more under a pixel. */
typedef struct {
    int32_t recip;
    bool sub;
} Slope;

static Slope slope_of(const int32_t h)
{
    if (h < 32768)
        return (Slope){(int32_t)((int64_t)(65536 << 14) / h), true};

    return (Slope){FXDIV(65536, h), false};
}

static int32_t slope_mul(const int32_t d, const Slope s)
{
    return s.sub ? FXMUL14(d, s.recip) : FXMUL(d, s.recip);
}

/* d over the height h, both 16.16, floored: an edge errs left by under a unit a row, so a centre
   on it keeps gfx's tie. qgl's reciprocal, truncated, drifted a pixel in a few hundred rows. Under
   a row, qgl's. */
static int32_t edge_slope(const int32_t d, const int32_t h)
{
    if (h < 65536)
        return slope_mul(d, slope_of(h));

    const int64_t n = (int64_t)d * 65536;
    return (int32_t)(n / h - (n % h < 0));
}

/* how far into the row a's y sits, to the next row: 16.16 */
static int32_t prestep(const int32_t ky) { return 65536 - (ky & 0xFFFF); }

/* qgl$drawA: u and v 16.16 down the left edge, a constant step a pixel. */
static int32_t draw_a(
    const Fx *const fx,
    const int32_t cnt,
    const Grad *const g,
    ScanFill *const fill,
    void *const ctx
)
{
    const double r65536 = 0.0000152587890625;
    int32_t lines = 0;
    int32_t lf_s = 0, lf_e = cnt - 1, rg_s = 0, rg_e = 1;
    const int32_t rg_lim = cnt - 1;
    int16_t lf_hgt = 0, rg_hgt = 0, height = 0;
    int16_t yy = (int16_t)FXFLOOR(fx[0].ky);
    int32_t lf_x = 0, lf_dxdy = 0, lf_u = 0, lf_dudy = 0, lf_v = 0, lf_dvdy = 0;
    int32_t rg_x = 0, rg_dxdy = 0;
    float lf_z = 0, lf_dzdy = 0;

    for (;;) {
        lf_hgt = (int16_t)(lf_hgt - height);

        if (lf_hgt < 0)
            break;

        if (lf_hgt == 0) {
            if (lf_e <= 0)
                break;

            const Fx *const b = &fx[lf_s], *const e = &fx[lf_e];
            lf_hgt = rows_between(b->ky, e->ky);

            if (lf_hgt < 0)
                break;

            if (lf_hgt > 0) {
                const int32_t h = e->ky - b->ky;
                lf_dxdy = edge_slope(e->kx - b->kx, h);
                lf_dudy = edge_slope(e->ku - b->ku, h);
                lf_dvdy = edge_slope(e->kv - b->kv, h);

                /* z by the fractional height, as x, u and v */
                lf_dzdy = real4(((double)e->kz - b->kz) * (65536.0 / (double)h));

                const int32_t f = prestep(b->ky) - g->bias;
                lf_x = add32(FXMUL(f, lf_dxdy), b->kx);
                lf_u = add32(FXMUL(f, lf_dudy), b->ku);
                lf_v = add32(FXMUL(f, lf_dvdy), b->kv);
                lf_z = real4((double)f * r65536 * lf_dzdy + b->kz);
            }

            lf_s = lf_e;
            lf_e--;
        }

        rg_hgt = (int16_t)(rg_hgt - height);

        if (rg_hgt < 0)
            break;

        if (rg_hgt == 0) {
            if (rg_e > rg_lim)
                break;

            const Fx *const b = &fx[rg_s], *const e = &fx[rg_e];
            rg_hgt = rows_between(b->ky, e->ky);

            if (rg_hgt < 0)
                break;

            if (rg_hgt > 0) {
                rg_dxdy = edge_slope(e->kx - b->kx, e->ky - b->ky);
                rg_x = add32(FXMUL(prestep(b->ky) - g->bias, rg_dxdy), b->kx);
            }

            rg_s = rg_e;
            rg_e++;
        }

        height = lf_hgt < rg_hgt ? lf_hgt : rg_hgt;

        if (height <= 0)
            continue;

        for (int16_t ycnt = height; ycnt > 0; ycnt--) {
            /* u and v at the centre of the first whole pixel the span covers */
            const int32_t edi = prestep(lf_x);
            const int32_t u = add32(add32(lf_u, 32768), FXMUL(edi, g->dudx));
            const int32_t v = add32(add32(lf_v, 32768), FXMUL(edi, g->dvdx));
            const int16_t x0 = (int16_t)FXFLOOR(lf_x);
            const int16_t width = (int16_t)(FXFLOOR(rg_x) - FXFLOOR(lf_x));

            if (width > 0) {
                const ScanSpan sp = {
                    .y = yy,
                    .x0 = x0,
                    .x1 = x0 + width,
                    .u = u,
                    .v = v,
                    .du = g->dudx,
                    .dv = g->dvdx,
                    .z = scan_fistp_low((double)lf_z * DEPTH_NEAR * 65536.0),
                    .dz = g->dzdx,
                };

                fill(ctx, &sp);
                lines++;
            }

            lf_x = add32(lf_x, lf_dxdy);
            lf_u = add32(lf_u, lf_dudy);
            lf_v = add32(lf_v, lf_dvdy);
            rg_x = add32(rg_x, rg_dxdy);
            lf_z = real4((double)lf_z + lf_dzdy);
            yy++;
        }
    }

    return lines;
}

/* qgl$drawP: u/z, v/z and 1/z floats down the left edge, x 16.16 as drawA's. */
static int32_t draw_p(
    const Fx *const fx,
    const int32_t cnt,
    const Grad *const g,
    ScanFill *const fill,
    void *const ctx
)
{
    const double r65536 = 0.0000152587890625;
    int32_t lines = 0;
    int32_t lf_s = 0, lf_e = cnt - 1, rg_s = 0, rg_e = 1;
    const int32_t rg_lim = cnt - 1;
    int16_t lf_hgt = 0, rg_hgt = 0, height = 0;
    int16_t yy = (int16_t)FXFLOOR(fx[0].ky);
    int32_t lf_x = 0, lf_dxdy = 0, rg_x = 0, rg_dxdy = 0;
    float lf_u = 0, lf_dudy = 0, lf_v = 0, lf_dvdy = 0, lf_z = 0, lf_dzdy = 0;

    for (;;) {
        lf_hgt = (int16_t)(lf_hgt - height);

        if (lf_hgt < 0)
            break;

        if (lf_hgt == 0) {
            if (lf_e <= 0)
                break;

            const Fx *const b = &fx[lf_s], *const e = &fx[lf_e];
            lf_hgt = rows_between(b->ky, e->ky);

            if (lf_hgt < 0)
                break;

            if (lf_hgt > 0) {
                /* one reciprocal of the fractional height serves u, v and z */
                const int32_t h = e->ky - b->ky;
                const double inv = 65536.0 / (double)h;
                lf_dxdy = edge_slope(e->kx - b->kx, h);
                lf_dudy = real4(((double)e->fu - b->fu) * inv);
                lf_dvdy = real4(((double)e->fv - b->fv) * inv);
                lf_dzdy = real4(((double)e->kz - b->kz) * inv);

                const int32_t pfrac = prestep(b->ky) - g->bias;
                const double diff = (double)pfrac * r65536;
                lf_x = add32(FXMUL(pfrac, lf_dxdy), b->kx);
                lf_u = real4((double)lf_dudy * diff + b->fu);
                lf_v = real4((double)lf_dvdy * diff + b->fv);
                lf_z = real4((double)lf_dzdy * diff + b->kz);
            }

            lf_s = lf_e;
            lf_e--;
        }

        rg_hgt = (int16_t)(rg_hgt - height);

        if (rg_hgt < 0)
            break;

        if (rg_hgt == 0) {
            if (rg_e > rg_lim)
                break;

            const Fx *const b = &fx[rg_s], *const e = &fx[rg_e];
            rg_hgt = rows_between(b->ky, e->ky);

            if (rg_hgt < 0)
                break;

            if (rg_hgt > 0) {
                rg_dxdy = edge_slope(e->kx - b->kx, e->ky - b->ky);
                rg_x = add32(FXMUL(prestep(b->ky) - g->bias, rg_dxdy), b->kx);
            }

            rg_s = rg_e;
            rg_e++;
        }

        height = lf_hgt < rg_hgt ? lf_hgt : rg_hgt;

        if (height <= 0)
            continue;

        for (int16_t ycnt = height; ycnt > 0; ycnt--) {
            const int32_t pfrac = prestep(lf_x);
            const int16_t x0 = (int16_t)FXFLOOR(lf_x);
            const int16_t width = (int16_t)(FXFLOOR(rg_x) - FXFLOOR(lf_x));

            if (width > 0) {
                /* u/z, v/z and 1/z at the first pixel centre */
                const double diff = (double)pfrac * r65536;
                const ScanSpan sp = {
                    .y = yy,
                    .x0 = x0,
                    .x1 = x0 + width,
                    .pu = (double)lf_u + (double)g->fdudx * diff,
                    .pv = (double)lf_v + (double)g->fdvdx * diff,
                    .pz = (double)lf_z + (double)g->fdzdx * diff,
                    .dpu = g->fdudx,
                    .dpv = g->fdvdx,
                    .dpz = g->fdzdx,
                    .dpun = real4((double)g->fdudx * SCAN_SUBDIVP),
                    .dpvn = real4((double)g->fdvdx * SCAN_SUBDIVP),
                    .dpzn = real4((double)g->fdzdx * SCAN_SUBDIVP),
                    .z = scan_fistp_low((double)lf_z * DEPTH_NEAR * 65536.0),
                    .dz = g->dzdx,
                };

                fill(ctx, &sp);
                lines++;
            }

            lf_x = add32(lf_x, lf_dxdy);
            rg_x = add32(rg_x, rg_dxdy);
            lf_u = real4((double)lf_u + lf_dudy);
            lf_v = real4((double)lf_v + lf_dvdy);
            lf_z = real4((double)lf_z + lf_dzdy);
            yy++;
        }
    }

    return lines;
}

int32_t scan_poly(
    const ScanVert *const v,
    const uint32_t n,
    const ScanMode mode,
    const Rect clip,
    const bool gfx,
    ScanFill *const fill,
    void *const ctx
)
{
    if (n < 3 || n > SCAN_MAXV || mode < SCAN_FLAT || mode > SCAN_ATEX)
        return -1;

    const bool persp = mode == SCAN_PTEX || mode == SCAN_ATEX;

    /* the triple: the vertex furthest from v[0] (L1), then the furthest off that baseline */
    uint32_t b = 1, c = 2;
    float best = 0;

    for (uint32_t i = 1; i < n; i++) {
        const double dx = (double)v[i].x - v[0].x, dy = (double)v[i].y - v[0].y;
        const float cur = real4((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy));

        if (cur > best) {
            best = cur;
            b = i;
        }
    }

    best = 0;

    for (uint32_t i = 1; i < n; i++) {
        if (i == b)
            continue;

        const double cross = nom(v[0].x, v[b].x, v[i].x, v[0].y, v[b].y, v[i].y);
        const float cur = real4(cross < 0 ? -cross : cross);

        if (cur > best) {
            best = cur;
            c = i;
        }
    }

    /* by ring index, so the denominator's sign is the ring's winding */
    const uint32_t lo = b < c ? b : c, hi = b < c ? c : b;
    Grad g = {};

    if (!grad(&v[0], &v[lo], &v[hi], persp, &g))
        return 0;

    /* POLY_CULL: twice the area under 2 is too small to matter; its sign is the winding */
    const double area2 = g.denom < 0 ? -g.denom : g.denom;

    if (!(area2 >= 2.0))
        return 0;

    const int32_t vstep = g.denom > 0 ? 1 : -1;
    uint32_t top = 0;

    for (uint32_t i = 1; i < n; i++)
        if (!(v[i].y >= v[top].y))
            top = i;

    /* the ring from the topmost vertex in the winding's direction, clipped */
    ScanVert ring[SCAN_MAXV + 4], work[SCAN_MAXV + 4];

    for (uint32_t k = 0; k < n; k++)
        ring[k] = v[((int32_t)top + vstep * (int32_t)k + (int32_t)n) % (int32_t)n];

    ScanVert *clipped = nullptr;
    const uint32_t cnt = clip_poly(ring, work, n, clip, &clipped);

    if (cnt < 3)
        return 0;

    Fx fx[SCAN_MAXV + 4], walk[SCAN_MAXV + 4];

    /* gfx's tie, a pixel centre on a top or left edge inside; qgl's are the bottom and right. The
       rows are a unit up, the edges met at the row itself (bias): a unit up there would move a
       steep edge more than the unit left. */
    const int32_t half = gfx && persp ? 32768 : 0;
    g.bias = gfx;

    for (uint32_t i = 0; i < cnt; i++) {
        fx[i] = persp ? to_fixed_p(&clipped[i]) : to_fixed_a(&clipped[i]);
        fx[i].kx += half - gfx;
        fx[i].ky += half - gfx;
    }

    /* the chains split at fx[0]: the topmost vertex, rotated to the front */
    uint32_t k = 0;

    for (uint32_t i = 1; i < cnt; i++)
        if (fx[i].ky < fx[k].ky)
            k = i;

    for (uint32_t i = 0; i < cnt; i++)
        walk[i] = fx[(i + k) % cnt];

    return persp ? draw_p(walk, (int32_t)cnt, &g, fill, ctx)
                 : draw_a(walk, (int32_t)cnt, &g, fill, ctx);
}
