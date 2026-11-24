/* qgl's convex scanner (scanner.h): rs.asm's qglRsPoly, qgl$Grad, qgl$F2fxA and P, qgl$drawA and P,
   and clip.asm's qglClPolyEx. */
#include "backend/soft/fill.h"
#include "backend/soft/scanner.h"

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

    const double gu16 = nom(a->u, b->u, c->u, a->y, b->y, c->y) * rdenom;
    g->fdudx = real4(gu16 * r65536);

    const double gv16 = nom(a->v, b->v, c->v, a->y, b->y, c->y) * rdenom;
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

/* qgl$F2fxA, and qgl$F2fxP with the half pixel it leaves out: x and y half a pixel on, less
   gfx's nudge (a unit up and left); u and v 16.16, or floats in perspective. */
static Fx to_fixed(const ScanVert *const v, const bool persp)
{
    Fx f = {
        .kx = scan_fistp(((double)v->x + 0.5) * 65536.0) - 1,
        .ky = scan_fistp(((double)v->y + 0.5) * 65536.0) - 1,
        .kz = v->z,
    };

    if (persp) {
        f.fu = real4(v->u);
        f.fv = real4(v->v);
    } else {
        f.ku = scan_fistp((double)v->u * 65536.0);
        f.kv = scan_fistp((double)v->v * 65536.0);
    }

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

/* A chain's edge: its top vertex s and its bottom e, the rows left, and x and its step a row,
   16.16 (qgl's lf_ and rg_). */
typedef struct {
    int32_t s, e;
    int16_t rows;
    int32_t x, dx;
} Edge;

/* What the left edge carries down a row at a time: u and v, 16.16 affine (qgl$drawA) or u/z and
   v/z floats in perspective (qgl$drawP); 1/z. */
typedef struct {
    int32_t u, du, v, dv;
    float pu, dpu, pv, dpv;
    float z, dz;
} Carried;

/* The chain's edge from b to e begun: false, it runs up and the walk ends. */
static bool edge_from(Edge *const ed, const Fx *const b, const Fx *const e, const Grad *const g)
{
    ed->rows = rows_between(b->ky, e->ky);

    if (ed->rows > 0) {
        ed->dx = edge_slope(e->kx - b->kx, e->ky - b->ky);
        ed->x = add32(FXMUL(prestep(b->ky) - g->bias, ed->dx), b->kx);
    }

    return ed->rows >= 0;
}

/* The left edge's u, v and 1/z from b to e, the edge a row or more high: met at the edge's first
   row, as its x. */
static void carried_from(
    Carried *const c,
    const Fx *const b,
    const Fx *const e,
    const Grad *const g,
    const bool persp
)
{
    const double r65536 = 0.0000152587890625;
    const int32_t h = e->ky - b->ky, f = prestep(b->ky) - g->bias;
    const double inv = 65536.0 / (double)h, diff = (double)f * r65536;

    c->dz = real4(((double)e->kz - b->kz) * inv);
    c->z = real4(diff * c->dz + b->kz);

    if (persp) {
        c->dpu = real4(((double)e->fu - b->fu) * inv);
        c->dpv = real4(((double)e->fv - b->fv) * inv);
        c->pu = real4((double)c->dpu * diff + b->fu);
        c->pv = real4((double)c->dpv * diff + b->fv);
        return;
    }

    c->du = edge_slope(e->ku - b->ku, h);
    c->dv = edge_slope(e->kv - b->kv, h);
    c->u = add32(FXMUL(f, c->du), b->ku);
    c->v = add32(FXMUL(f, c->dv), b->kv);
}

/* The left edge a row down. */
static void carried_on(Carried *const c, const bool persp)
{
    c->z = real4((double)c->z + c->dz);

    if (persp) {
        c->pu = real4((double)c->pu + c->dpu);
        c->pv = real4((double)c->pv + c->dpv);
    } else {
        c->u = add32(c->u, c->du);
        c->v = add32(c->v, c->dv);
    }
}

/* Row y's span from the left edge's x to the right's, its u, v and 1/z taken at the first pixel
   centre; then held in the clip, its values a pixel on for each it lost. False: none. */
static bool span_of(
    const Edge *const l,
    const Edge *const r,
    const Carried *const c,
    const Grad *const g,
    const bool persp,
    const int16_t y,
    const Rect clip,
    ScanSpan *const sp
)
{
    const double r65536 = 0.0000152587890625;
    const int32_t into = prestep(l->x);
    const int16_t x0 = (int16_t)FXFLOOR(l->x);
    const int16_t width = (int16_t)(FXFLOOR(r->x) - FXFLOOR(l->x));
    const double diff = (double)into * r65536;

    if (width <= 0)
        return false;

    *sp = (ScanSpan){
        .y = y,
        .x0 = x0,
        .x1 = x0 + width,
        .z = scan_fistp_low((double)c->z * DEPTH_NEAR * 65536.0),
        .dz = g->dzdx,
    };

    if (persp) {
        sp->pu = (double)c->pu + (double)g->fdudx * diff;
        sp->pv = (double)c->pv + (double)g->fdvdx * diff;
        sp->pz = (double)c->z + (double)g->fdzdx * diff;
        sp->dpu = g->fdudx;
        sp->dpv = g->fdvdx;
        sp->dpz = g->fdzdx;
        sp->dpun = real4((double)g->fdudx * SCAN_SUBDIVP);
        sp->dpvn = real4((double)g->fdvdx * SCAN_SUBDIVP);
        sp->dpzn = real4((double)g->fdzdx * SCAN_SUBDIVP);
    } else {
        sp->u = add32(add32(c->u, 32768), FXMUL(into, g->dudx));
        sp->v = add32(add32(c->v, 32768), FXMUL(into, g->dvdx));
        sp->du = g->dudx;
        sp->dv = g->dvdx;
    }

    const int32_t from = rect_max(sp->x0, clip.x), to = rect_min(sp->x1, clip.x + clip.w);
    const int32_t lost = from - sp->x0;

    if (y < clip.y || y >= clip.y + clip.h || from >= to)
        return false;

    sp->x0 = from;
    sp->x1 = to;
    sp->z = depth_on(sp->z, sp->dz, lost);
    sp->u = (int32_t)(sp->u + (int64_t)lost * sp->du);
    sp->v = (int32_t)(sp->v + (int64_t)lost * sp->dv);
    sp->pu += sp->dpu * (double)lost;
    sp->pv += sp->dpv * (double)lost;
    sp->pz += sp->dpz * (double)lost;
    return true;
}

/* qgl$drawA and qgl$drawP: the two chains down from fx[0], the topmost vertex, left the ring's
   way back and right its way on, one span a row between them. */
static void walk(
    const Fx *const fx,
    const int32_t cnt,
    const Grad *const g,
    const bool persp,
    const Rect clip,
    ScanFill *const fill,
    void *const ctx
)
{
    Edge l = {.s = 0, .e = cnt - 1}, r = {.s = 0, .e = 1};

    Carried c = {};

    int16_t height = 0, y = (int16_t)FXFLOOR(fx[0].ky);

    for (;;) {
        l.rows = (int16_t)(l.rows - height);
        r.rows = (int16_t)(r.rows - height);

        if (l.rows < 0)
            return;

        if (l.rows == 0) {
            if (l.e <= 0 || !edge_from(&l, &fx[l.s], &fx[l.e], g))
                return;

            if (l.rows > 0)
                carried_from(&c, &fx[l.s], &fx[l.e], g, persp);

            l.s = l.e--;
        }

        if (r.rows < 0)
            return;

        if (r.rows == 0) {
            if (r.e > cnt - 1 || !edge_from(&r, &fx[r.s], &fx[r.e], g))
                return;

            r.s = r.e++;
        }

        height = l.rows < r.rows ? l.rows : r.rows;

        for (int16_t row = 0; row < height; row++, y++) {
            ScanSpan sp;

            if (span_of(&l, &r, &c, g, persp, y, clip, &sp))
                fill(ctx, &sp);

            l.x = add32(l.x, l.dx);
            r.x = add32(r.x, r.dx);
            carried_on(&c, persp);
        }
    }
}

void scan_poly(
    const ScanVert *const v,
    const uint32_t n,
    const ScanMode mode,
    const Rect clip,
    ScanFill *const fill,
    void *const ctx
)
{
    if (n < 3 || n > SCAN_MAXV)
        return;

    const bool persp = mode == SCAN_PTEX;

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
        return;

    /* POLY_CULL: twice the area under 2 is too small to matter; its sign is the winding */
    const double area2 = g.denom < 0 ? -g.denom : g.denom;

    if (!(area2 >= 2.0))
        return;

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
        return;

    Fx fx[SCAN_MAXV + 4], ring_fx[SCAN_MAXV + 4];

    /* gfx's tie, a pixel centre on a top or left edge inside; qgl's are the bottom and right. The
       rows are a unit up, the edges met at the row itself (bias): a unit up there would move a
       steep edge more than the unit left. */
    g.bias = 1;

    for (uint32_t i = 0; i < cnt; i++)
        fx[i] = to_fixed(&clipped[i], persp);

    /* the chains split at fx[0]: the topmost vertex, rotated to the front */
    uint32_t k = 0;

    for (uint32_t i = 1; i < cnt; i++)
        if (fx[i].ky < fx[k].ky)
            k = i;

    for (uint32_t i = 0; i < cnt; i++)
        ring_fx[i] = fx[(i + k) % cnt];

    walk(ring_fx, (int32_t)cnt, &g, persp, clip, fill, ctx);
}
