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

/* Every x and y finite, by their exponents: the clip cannot hold a ring with one that is not, and
   the fillers test no bound. */
static bool finite_xy(const ScanVert *const v, const uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        uint32_t x, y;
        __builtin_memcpy(&x, &v[i].x, sizeof x);
        __builtin_memcpy(&y, &v[i].y, sizeof y);

        if ((x & 0x7F800000) == 0x7F800000 || (y & 0x7F800000) == 0x7F800000)
            return false;
    }

    return true;
}

/* mgl's outcodes of n vertices against clip, each by clip_pass's test, its distance's sign,
   which real4 keeps: in all, those outside a boundary; in any, outside one. */
static void outcodes(
    const ScanVert *const v,
    const uint32_t n,
    const Rect clip,
    uint32_t *const all,
    uint32_t *const any
)
{
    const float bound[4] = {
        (float)clip.x,
        (float)(clip.x + clip.w),
        (float)clip.y,
        (float)(clip.y + clip.h),
    };

    *all = 15;
    *any = 0;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t out = 0;

        for (uint32_t pass = 0; pass < 4; pass++) {
            const float at = pass >> 1 ? v[i].y : v[i].x;

            if (__builtin_signbit(((double)at - bound[pass]) * (pass & 1 ? -1.0f : 1.0f)))
                out |= 1u << pass;
        }

        *all &= out;
        *any |= out;
    }
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

/* n / d and its remainder, the quotient a 32-bit one: on i386 one idiv, not __alldiv's call. */
static int32_t div_of(const int64_t n, const int32_t d, int32_t *const rem)
{
#ifdef __i386__
    int32_t q;
    __asm__("idivl %[d]" : "=a"(q), "=d"(*rem) : "A"(n), [d] "rm"(d) : "cc");
    return q;
#else

    *rem = (int32_t)(n % d);
    return (int32_t)(n / d);
#endif
}

static Slope slope_of(const int32_t h)
{
    int32_t rem;

    if (h < 32768)
        return (Slope){div_of(65536 << 14, h, &rem), true};

    return (Slope){div_of((int64_t)65536 << 16, h, &rem), false};
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

    int32_t rem;
    const int32_t q = div_of((int64_t)d * 65536, h, &rem);

    return q - (rem < 0);
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

/* The left edge's 1/z from b to e, the edge a row or more high: met at the edge's first row, as
   its x. */
static void carried_depth(
    Carried *const c,
    const Fx *const b,
    const Fx *const e,
    const Grad *const g
)
{
    const double r65536 = 0.0000152587890625;
    const int32_t h = e->ky - b->ky, f = prestep(b->ky) - g->bias;
    const double inv = 65536.0 / (double)h, diff = (double)f * r65536;

    c->dz = real4(((double)e->kz - b->kz) * inv);
    c->z = real4(diff * c->dz + b->kz);
}

/* carried_depth's when deep, and u and v. */
[[gnu::always_inline]]
static inline void carried_from(
    Carried *const c,
    const Fx *const b,
    const Fx *const e,
    const Grad *const g,
    const bool persp,
    const bool deep
)
{
    const double r65536 = 0.0000152587890625;
    const int32_t h = e->ky - b->ky, f = prestep(b->ky) - g->bias;
    const double inv = 65536.0 / (double)h, diff = (double)f * r65536;

    if (deep)
        carried_depth(c, b, e, g);

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

/* The left edge a row down; its depth when deep. */
[[gnu::always_inline]]
static inline void carried_on(Carried *const c, const bool persp, const bool deep)
{
    if (deep)
        c->z = real4((double)c->z + c->dz);

    if (persp) {
        c->pu = real4((double)c->pu + c->dpu);
        c->pv = real4((double)c->pv + c->dpv);
    } else {
        c->u = add32(c->u, c->du);
        c->v = add32(c->v, c->dv);
    }
}

/* A polygon set up to walk: its ring of 16.16 vertices from the topmost, clipped, and its
   gradients. */
typedef struct {
    Fx fx[SCAN_MAXV + 4];
    int32_t cnt;
    Grad g;
} Ring;

/* qglRsPoly's front half: n vertices to ring, false when culled or clipped away. */
static bool ring_of(
    const ScanVert *const v,
    const uint32_t n,
    const bool persp,
    const Rect clip,
    Ring *const ring
)
{
    if (n < 3 || n > SCAN_MAXV || !finite_xy(v, n))
        return false;

    /* every vertex outside one boundary: clip_poly would keep none; inside all, each as it is */
    uint32_t all, any;
    outcodes(v, n, clip, &all, &any);

    if (all)
        return false;

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
    Grad *const g = &ring->g;
    __builtin_memset(g, 0, sizeof *g);

    if (!grad(&v[0], &v[lo], &v[hi], persp, g))
        return false;

    /* POLY_CULL: twice the area under 2 is too small to matter; its sign is the winding */
    const double area2 = g->denom < 0 ? -g->denom : g->denom;

    if (!(area2 >= 2.0))
        return false;

    const int32_t vstep = g->denom > 0 ? 1 : -1;
    uint32_t top = 0;

    for (uint32_t i = 1; i < n; i++)
        if (!(v[i].y >= v[top].y))
            top = i;

    /* the ring from the topmost vertex in the winding's direction, clipped */
    ScanVert from[SCAN_MAXV + 4], work[SCAN_MAXV + 4];

    for (uint32_t k = 0, at = top; k < n; k++) {
        from[k] = v[at];
        at = vstep > 0 ? (at + 1 == n ? 0 : at + 1) : (at == 0 ? n - 1 : at - 1);
    }

    ScanVert *clipped = from;
    const uint32_t cnt = any ? clip_poly(from, work, n, clip, &clipped) : n;

    if (cnt < 3)
        return false;

    Fx fx[SCAN_MAXV + 4];

    /* gfx's tie, a pixel centre on a top or left edge inside; qgl's are the bottom and right. The
       rows are a unit up, the edges met at the row itself (bias): a unit up there would move a
       steep edge more than the unit left. */
    g->bias = 1;

    for (uint32_t i = 0; i < cnt; i++)
        fx[i] = to_fixed(&clipped[i], persp);

    /* the chains split at fx[0]: the topmost vertex, rotated to the front */
    uint32_t k = 0;

    for (uint32_t i = 1; i < cnt; i++)
        if (fx[i].ky < fx[k].ky)
            k = i;

    for (uint32_t i = k; i < cnt; i++)
        ring->fx[i - k] = fx[i];

    for (uint32_t i = 0; i < k; i++)
        ring->fx[cnt - k + i] = fx[i];

    ring->cnt = (int32_t)cnt;
    return true;
}

/* qgl$drawA's walk, the left edge carrying 1/z: each row's run from the left edge's pixel up to
   the right's, its depth at its first pixel centre, filled by fill. */
void scan_flat(
    const ScanVert *const v,
    const uint32_t n,
    const Rect clip,
    Rows *const r,
    RowFlat *const fill
)
{
    Ring ring;

    if (!ring_of(v, n, false, clip, &ring))
        return;

    const Fx *const fx = ring.fx;
    const int32_t cnt = ring.cnt;
    const Grad *const g = &ring.g;

    r->dz = g->dzdx;

    Edge l = {.s = 0, .e = cnt - 1}, rt = {.s = 0, .e = 1};

    Carried c = {};

    int16_t height = 0, y = (int16_t)FXFLOOR(fx[0].ky);

    for (;;) {
        l.rows = (int16_t)(l.rows - height);
        rt.rows = (int16_t)(rt.rows - height);

        if (l.rows < 0)
            return;

        if (l.rows == 0) {
            if (l.e <= 0 || !edge_from(&l, &fx[l.s], &fx[l.e], g))
                return;

            if (l.rows > 0)
                carried_depth(&c, &fx[l.s], &fx[l.e], g);

            l.s = l.e--;
        }

        if (rt.rows < 0)
            return;

        if (rt.rows == 0) {
            if (rt.e > cnt - 1 || !edge_from(&rt, &fx[rt.s], &fx[rt.e], g))
                return;

            rt.s = rt.e++;
        }

        height = l.rows < rt.rows ? l.rows : rt.rows;

        for (int16_t row = 0; row < height; row++, y++) {
            const int32_t x0 = (int16_t)FXFLOOR(l.x);
            const int32_t x1 = x0 + (int16_t)(FXFLOOR(rt.x) - FXFLOOR(l.x));

            if (x1 > x0)
                fill(r, y, x0, x1, scan_fistp_low((double)c.z * DEPTH_NEAR * 65536.0));

            l.x = add32(l.x, l.dx);
            rt.x = add32(rt.x, rt.dx);
            c.z = real4((double)c.z + c.dz);
        }
    }
}

#if defined(__i386__)
/* affine_walk's rows without depth in asm, uGL's FILLSEL, by additions alone: each row's run and
   its u and v at its first pixel as affine_walk's Across; picked, the plain filler or the held one
   by steps_inside's 32-bit test. x0 is in edi, x1 in esi, u in ebp and v in ebx, which the filler
   keeps; the rest of the walk is in rows_walk, at its absolute place. A fraction is in its
   register's upper 16 bits, so its add carries into the whole part's adc. */
typedef struct {
    uint32_t fl, fdl;     /* the left edge's fraction and its step's */
    int32_t il;           /* its whole step a row */
    uint32_t fr, fdr;     /* the right edge's */
    int32_t ir;
    int32_t x0, x1;       /* the first row's run */
    uint32_t uf, usf;     /* u at x0's fraction, and its step's */
    int32_t ui, usi;      /* u at x0, 16.16, and its step's, less the left edge's carry */
    uint32_t vf, vsf;
    int32_t vi, vsi;
    int32_t ue, uw;       /* Across's end, and its step less the edges' carries */
    int32_t ve, vw;
    int32_t dudx, dvdx;
    int32_t y, rows;      /* the row, and the rows left */
    uint32_t sw, sh;      /* the texture's sides, 16.16 */
    Rows *r;
    RowAffine *plain, *held;
} RowsWalk;

/* clang-format off */
/* RowsWalk's places, the asm's names for them. */
#define RW_fl 0
#define RW_fdl 4
#define RW_il 8
#define RW_fr 12
#define RW_fdr 16
#define RW_ir 20
#define RW_x0 24
#define RW_x1 28
#define RW_uf 32
#define RW_usf 36
#define RW_ui 40
#define RW_usi 44
#define RW_vf 48
#define RW_vsf 52
#define RW_vi 56
#define RW_vsi 60
#define RW_ue 64
#define RW_uw 68
#define RW_ve 72
#define RW_vw 76
#define RW_dudx 80
#define RW_dvdx 84
#define RW_y 88
#define RW_rows 92
#define RW_sw 96
#define RW_sh 100
#define RW_r 104
#define RW_plain 108
#define RW_held 112
#define RW_CHECK(f) static_assert(__builtin_offsetof(RowsWalk, f) == RW_##f);
RW_CHECK(fl) RW_CHECK(fdl) RW_CHECK(il) RW_CHECK(fr) RW_CHECK(fdr) RW_CHECK(ir) RW_CHECK(x0)
RW_CHECK(x1) RW_CHECK(uf) RW_CHECK(usf) RW_CHECK(ui) RW_CHECK(usi) RW_CHECK(vf) RW_CHECK(vsf)
RW_CHECK(vi) RW_CHECK(vsi) RW_CHECK(ue) RW_CHECK(uw) RW_CHECK(ve) RW_CHECK(vw) RW_CHECK(dudx)
RW_CHECK(dvdx) RW_CHECK(y) RW_CHECK(rows) RW_CHECK(sw) RW_CHECK(sh) RW_CHECK(r) RW_CHECK(plain)
RW_CHECK(held)
#define RW_STR(n) #n
#define RW_AT(n) RW_STR(n)
/* Field f of rows_walk, operand [w]. */
#define RW(f) RW_AT(RW_##f) "+%[w]"

/* The row skipped, 9f, unless it shows. The ring is clipped, so its rows are inside. */
#define ROWS_RUN                                                                                   \
    "cmpl %%edi, %%esi\n\t"                                                                        \
    "jle 9f\n\t"
/* The filler in ecx: plain when the walk of x1 - x0 pixels stays inside the texture. */
#define ROWS_PICK                                                                                  \
    "movl " RW(held) ", %%ecx\n\t"                                                                 \
    "cmpl " RW(sw) ", %%ebp\n\t"                                                                   \
    "jae 4f\n\t"                                                                                   \
    "movl %%ebp, %%eax\n\t"                                                                        \
    "addl " RW(ue) ", %%eax\n\t"                                                                   \
    "jo 4f\n\t"                                                                                    \
    "cmpl " RW(sw) ", %%eax\n\t"                                                                   \
    "jae 4f\n\t"                                                                                   \
    "cmpl " RW(sh) ", %%ebx\n\t"                                                                   \
    "jae 4f\n\t"                                                                                   \
    "movl %%ebx, %%eax\n\t"                                                                        \
    "addl " RW(ve) ", %%eax\n\t"                                                                   \
    "jo 4f\n\t"                                                                                    \
    "cmpl " RW(sh) ", %%eax\n\t"                                                                   \
    "jae 4f\n\t"                                                                                   \
    "movl " RW(plain) ", %%ecx\n"                                                                  \
    "4:\n\t"
/* fill(r, y, x0, x1, u, v, 0) */
#define ROWS_FILL                                                                                  \
    "pushl $0\n\t"                                                                                 \
    "pushl %%ebx\n\t"                                                                              \
    "pushl %%ebp\n\t"                                                                              \
    "pushl %%esi\n\t"                                                                              \
    "pushl %%edi\n\t"                                                                              \
    "pushl " RW(y) "\n\t"                                                                          \
    "pushl " RW(r) "\n\t"                                                                          \
    "call *%%ecx\n\t"                                                                              \
    "addl $28, %%esp\n"                                                                            \
    "9:\n\t"
/* The left edge a row down, edx less its carry; u and v with it. */
#define ROWS_LEFT                                                                                  \
    "movl " RW(fdl) ", %%eax\n\t"                                                                  \
    "addl %%eax, " RW(fl) "\n\t"                                                                   \
    "sbbl %%edx, %%edx\n\t"                                                                        \
    "adcl " RW(il) ", %%edi\n\t"                                                                   \
    "movl " RW(usf) ", %%eax\n\t"                                                                  \
    "addl %%eax, " RW(uf) "\n\t"                                                                   \
    "adcl " RW(usi) ", %%ebp\n\t"                                                                  \
    "movl " RW(vsf) ", %%eax\n\t"                                                                  \
    "addl %%eax, " RW(vf) "\n\t"                                                                   \
    "adcl " RW(vsi) ", %%ebx\n\t"                                                                  \
    "movl " RW(dudx) ", %%eax\n\t"                                                                 \
    "andl %%edx, %%eax\n\t"                                                                        \
    "addl %%eax, %%ebp\n\t"                                                                        \
    "movl " RW(dvdx) ", %%eax\n\t"                                                                 \
    "andl %%edx, %%eax\n\t"                                                                        \
    "addl %%eax, %%ebx\n\t"
/* The right edge, ecx less its carry. */
#define ROWS_RIGHT                                                                                 \
    "movl " RW(fdr) ", %%eax\n\t"                                                                  \
    "addl %%eax, " RW(fr) "\n\t"                                                                   \
    "sbbl %%ecx, %%ecx\n\t"                                                                        \
    "adcl " RW(ir) ", %%esi\n\t"
/* The run's ends a row down. */
#define ROWS_ENDS                                                                                  \
    "movl " RW(dudx) ", %%eax\n\t"                                                                 \
    "andl %%ecx, %%eax\n\t"                                                                        \
    "addl " RW(uw) ", %%eax\n\t"                                                                   \
    "addl %%eax, " RW(ue) "\n\t"                                                                   \
    "movl " RW(dudx) ", %%eax\n\t"                                                                 \
    "andl %%edx, %%eax\n\t"                                                                        \
    "subl %%eax, " RW(ue) "\n\t"                                                                   \
    "movl " RW(dvdx) ", %%eax\n\t"                                                                 \
    "andl %%ecx, %%eax\n\t"                                                                        \
    "addl " RW(vw) ", %%eax\n\t"                                                                   \
    "addl %%eax, " RW(ve) "\n\t"                                                                   \
    "movl " RW(dvdx) ", %%eax\n\t"                                                                 \
    "andl %%edx, %%eax\n\t"                                                                        \
    "subl %%eax, " RW(ve) "\n\t"
#define ROWS_LOOP                                                                                  \
    "incl " RW(y) "\n\t"                                                                           \
    "decl " RW(rows) "\n\t"                                                                        \
    "jnz 1b"
/* clang-format on */

static RowsWalk rows_walk;

/* rows_walk's rows, one or more, each picking its filler or each calling plain. */
#define ROWS_ASM(BODY)                                                                             \
    __asm__ volatile("movl " RW(x0) ", %%edi\n\t"                                                  \
                     "movl " RW(x1) ", %%esi\n\t"                                                  \
                     "movl " RW(ui) ", %%ebp\n\t"                                                  \
                     "movl " RW(vi) ", %%ebx\n"                                                    \
                     "1:\n\t" BODY                                                                 \
                     : [w] "+m"(rows_walk)                                                         \
                     :                                                                             \
                     : "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "cc", "memory", "st",      \
                       "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)")

static void rows_picked(void)
{
    ROWS_ASM(ROWS_RUN ROWS_PICK ROWS_FILL ROWS_LEFT ROWS_RIGHT ROWS_ENDS ROWS_LOOP);
}

static void rows_plain(void)
{
    ROWS_ASM(ROWS_RUN "movl " RW(plain) ", %%ecx\n\t" ROWS_FILL ROWS_LEFT ROWS_RIGHT ROWS_LOOP);
}
#endif

/* A row's u or v at its first pixel centre, 16.16 in at's upper 48 bits, and its last pixel's
   less its first's, end: each a row down by additions alone. at is the left edge's value, 32768
   and the prestep into the row times the gradient, the product kept whole so that its floor is
   the multiply's; a row on, the prestep loses the edge's fraction and, when it carries, gains a
   pixel. end is the span less a pixel times the gradient, and the span moves by the edges'
   whole steps and carries. */
typedef struct {
    int64_t at, step, carry;
    int64_t end, wide, grad;
} Across;

static Across across_from(
    const int32_t edge,
    const int32_t down,
    const Edge l,
    const Edge rt,
    const int32_t span,
    const int32_t grad
)
{
    return (Across){
        .at = (int64_t)edge * 65536 + (int64_t)prestep(l.x) * grad + (int64_t)32768 * 65536,
        .step = (int64_t)down * 65536 - (int64_t)(l.dx & 0xFFFF) * grad,
        .carry = (int64_t)grad * 65536,
        .end = (int64_t)(span - 1) * grad,
        .wide = (int64_t)((rt.dx >> 16) - (l.dx >> 16)) * grad,
        .grad = grad,
    };
}

static inline void across_on(Across *const a, const bool cl, const bool cr)
{
    a->at += a->step + (cl ? a->carry : 0);
    a->end += a->wide + (cr ? a->grad : 0) - (cl ? a->grad : 0);
}

/* qgl$drawA: the two chains down from the topmost vertex, each row's run from the left edge's
   pixel up to the right's, its u, v and, when deep, depth at its first pixel centre; filled by
   plain, or held where the walk leaves the texture. */
[[gnu::always_inline]]
static inline void affine_walk(
    const ScanVert *const v,
    const uint32_t n,
    const Rect clip,
    Rows *const r,
    RowAffine *const plain,
    RowAffine *const held,
    const bool deep
)
{
    Ring ring;

    if (!ring_of(v, n, false, clip, &ring))
        return;

    const Fx *const fx = ring.fx;
    const int32_t cnt = ring.cnt;
    const Grad *const g = &ring.g;

    r->du = g->dudx;
    r->dv = g->dvdx;
    r->dz = g->dzdx;
#if defined(__i386__)
    /* in asm unless deep or a side reaches 32768, where steps_inside's test needs 64 bits */
    const bool in_asm = !deep && r->src.w < 32768 && r->src.h < 32768;

    /* a run's ends in 32 bits, as steps_inside's test: else held, whose pixels are plain's */
    const int64_t gu = g->dudx < 0 ? -(int64_t)g->dudx : g->dudx;
    const int64_t gv = g->dvdx < 0 ? -(int64_t)g->dvdx : g->dvdx;
    const bool wide = (int64_t)(clip.w + 2) * (gu > gv ? gu : gv) >= INT32_MAX;
    void (*const walk_rows)(void) = plain == held || wide ? rows_plain : rows_picked;

    if (in_asm) {
        rows_walk.dudx = g->dudx;
        rows_walk.dvdx = g->dvdx;
        rows_walk.sw = (uint32_t)r->src.w << 16;
        rows_walk.sh = (uint32_t)r->src.h << 16;
        rows_walk.r = r;
        rows_walk.plain = wide ? held : plain;
        rows_walk.held = held;
    }
#endif

    Edge l = {.s = 0, .e = cnt - 1}, rt = {.s = 0, .e = 1};

    Carried c = {};

    int16_t height = 0, y = (int16_t)FXFLOOR(fx[0].ky);

    for (;;) {
        l.rows = (int16_t)(l.rows - height);
        rt.rows = (int16_t)(rt.rows - height);

        if (l.rows < 0)
            return;

        if (l.rows == 0) {
            if (l.e <= 0 || !edge_from(&l, &fx[l.s], &fx[l.e], g))
                return;

            if (l.rows > 0)
                carried_from(&c, &fx[l.s], &fx[l.e], g, false, deep);

            l.s = l.e--;
        }

        if (rt.rows < 0)
            return;

        if (rt.rows == 0) {
            if (rt.e > cnt - 1 || !edge_from(&rt, &fx[rt.s], &fx[rt.e], g))
                return;

            rt.s = rt.e++;
        }

        height = l.rows < rt.rows ? l.rows : rt.rows;

        if (height <= 0)
            continue;

        int32_t x0 = (int16_t)FXFLOOR(l.x), x1 = x0 + (int16_t)(FXFLOOR(rt.x) - FXFLOOR(l.x));
        Across u = across_from(c.u, c.du, l, rt, x1 - x0, g->dudx);
        Across vv = across_from(c.v, c.dv, l, rt, x1 - x0, g->dvdx);
#if defined(__i386__)

        if (in_asm) {
            rows_walk.fl = (uint32_t)l.x << 16;
            rows_walk.fdl = (uint32_t)l.dx << 16;
            rows_walk.il = l.dx >> 16;
            rows_walk.fr = (uint32_t)rt.x << 16;
            rows_walk.fdr = (uint32_t)rt.dx << 16;
            rows_walk.ir = rt.dx >> 16;
            rows_walk.x0 = x0;
            rows_walk.x1 = x1;
            rows_walk.uf = (uint32_t)u.at << 16;
            rows_walk.usf = (uint32_t)u.step << 16;
            rows_walk.ui = (int32_t)(u.at >> 16);
            rows_walk.usi = (int32_t)(u.step >> 16);
            rows_walk.vf = (uint32_t)vv.at << 16;
            rows_walk.vsf = (uint32_t)vv.step << 16;
            rows_walk.vi = (int32_t)(vv.at >> 16);
            rows_walk.vsi = (int32_t)(vv.step >> 16);
            rows_walk.ue = (int32_t)u.end;
            rows_walk.uw = (int32_t)u.wide;
            rows_walk.ve = (int32_t)vv.end;
            rows_walk.vw = (int32_t)vv.wide;
            rows_walk.y = y;
            rows_walk.rows = height;
            walk_rows();
            l.x = add32(l.x, (int32_t)((uint32_t)l.dx * (uint32_t)height));
            rt.x = add32(rt.x, (int32_t)((uint32_t)rt.dx * (uint32_t)height));
            c.u = add32(c.u, (int32_t)((uint32_t)c.du * (uint32_t)height));
            c.v = add32(c.v, (int32_t)((uint32_t)c.dv * (uint32_t)height));
            y = (int16_t)(y + height);
            continue;
        }
#endif

        for (int16_t row = 0; row < height; row++, y++) {
            if (x1 > x0) {
                const int32_t u0 = (int32_t)(u.at >> 16), v0 = (int32_t)(vv.at >> 16);
                const int32_t z = deep ? scan_fistp_low((double)c.z * DEPTH_NEAR * 65536.0) : 0;
                RowAffine *const fill = steps_in(u0, r->src.w) && steps_in(u0 + u.end, r->src.w) &&
                                                steps_in(v0, r->src.h) &&
                                                steps_in(v0 + vv.end, r->src.h)
                                            ? plain
                                            : held;

                fill(r, y, x0, x1, u0, v0, z);
            }

            const bool cl = (l.x & 0xFFFF) + (l.dx & 0xFFFF) > 0xFFFF;
            const bool cr = (rt.x & 0xFFFF) + (rt.dx & 0xFFFF) > 0xFFFF;

            l.x = add32(l.x, l.dx);
            rt.x = add32(rt.x, rt.dx);
            x0 += (l.dx >> 16) + cl;
            x1 += (rt.dx >> 16) + cr;
            across_on(&u, cl, cr);
            across_on(&vv, cl, cr);
            carried_on(&c, false, deep);
        }
    }
}

void scan_affine(
    const ScanVert *const v,
    const uint32_t n,
    const Rect clip,
    Rows *const r,
    RowAffine *const plain,
    RowAffine *const held
)
{
    if (r->depths)
        affine_walk(v, n, clip, r, plain, held, true);
    else
        affine_walk(v, n, clip, r, plain, held, false);
}

/* qgl$drawP: scan_affine's walk, the left edge carrying u/z, v/z and 1/z; each row's run filled
   by fill, which divides. */
void scan_persp(
    const ScanVert *const v,
    const uint32_t n,
    const Rect clip,
    Rows *const r,
    RowPersp *const fill
)
{
    const double r65536 = 0.0000152587890625;
    Ring ring;

    if (!ring_of(v, n, true, clip, &ring))
        return;

    const Fx *const fx = ring.fx;
    const int32_t cnt = ring.cnt;
    const Grad *const g = &ring.g;

    r->dz = g->dzdx;
    r->dpun = real4((double)g->fdudx * SCAN_SUBDIVP);
    r->dpvn = real4((double)g->fdvdx * SCAN_SUBDIVP);
    r->dpzn = real4((double)g->fdzdx * SCAN_SUBDIVP);

    Edge l = {.s = 0, .e = cnt - 1}, rt = {.s = 0, .e = 1};

    Carried c = {};

    int16_t height = 0, y = (int16_t)FXFLOOR(fx[0].ky);

    for (;;) {
        l.rows = (int16_t)(l.rows - height);
        rt.rows = (int16_t)(rt.rows - height);

        if (l.rows < 0)
            return;

        if (l.rows == 0) {
            if (l.e <= 0 || !edge_from(&l, &fx[l.s], &fx[l.e], g))
                return;

            if (l.rows > 0)
                carried_from(&c, &fx[l.s], &fx[l.e], g, true, true);

            l.s = l.e--;
        }

        if (rt.rows < 0)
            return;

        if (rt.rows == 0) {
            if (rt.e > cnt - 1 || !edge_from(&rt, &fx[rt.s], &fx[rt.e], g))
                return;

            rt.s = rt.e++;
        }

        height = l.rows < rt.rows ? l.rows : rt.rows;

        for (int16_t row = 0; row < height; row++, y++) {
            const int32_t into = prestep(l.x), x0 = (int16_t)FXFLOOR(l.x);
            const int32_t x1 = x0 + (int16_t)(FXFLOOR(rt.x) - FXFLOOR(l.x));

            if (x1 > x0) {
                const double diff = (double)into * r65536;

                fill(
                    r,
                    y,
                    x0,
                    x1,
                    (double)c.pu + (double)g->fdudx * diff,
                    (double)c.pv + (double)g->fdvdx * diff,
                    (double)c.z + (double)g->fdzdx * diff,
                    scan_fistp_low((double)c.z * DEPTH_NEAR * 65536.0)
                );
            }

            l.x = add32(l.x, l.dx);
            rt.x = add32(rt.x, rt.dx);
            carried_on(&c, true, true);
        }
    }
}
