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
    if (n < 3 || n > SCAN_MAXV)
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
    *g = (Grad){};

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

    for (uint32_t k = 0; k < n; k++)
        from[k] = v[((int32_t)top + vstep * (int32_t)k + (int32_t)n) % (int32_t)n];

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

    for (uint32_t i = 0; i < cnt; i++)
        ring->fx[i] = fx[(i + k) % cnt];

    ring->cnt = (int32_t)cnt;
    return true;
}

/* qgl$drawA's walk, the left edge carrying 1/z: each row's run from the left edge's pixel up to
   the right's, its depth at its first pixel centre, held in the clip, filled by fill. */
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
            const int32_t from = rect_max(x0, clip.x), to = rect_min(x1, clip.x + clip.w);

            if (x1 > x0 && y >= clip.y && y < clip.y + clip.h && from < to) {
                const int32_t z = scan_fistp_low((double)c.z * DEPTH_NEAR * 65536.0);

                fill(r, y, from, to, depth_on(z, g->dzdx, from - x0));
            }

            l.x = add32(l.x, l.dx);
            rt.x = add32(rt.x, rt.dx);
            c.z = real4((double)c.z + c.dz);
        }
    }
}

#if defined(__i386__)
/* affine_walk's rows without depth in asm, uGL's FILLSEL: each row's run and its u and v at its
   first pixel as affine_walk's, the plain filler or the held one picked by steps_inside's 32-bit
   test, then called. The filler takes every register but ebx, esi, edi and ebp, so the walk's
   state is in rows_walk, at its absolute place. */
typedef struct {
    int32_t lx, ldx, rx, rdx; /* the edges' x, 16.16, and their steps a row */
    int32_t u, du, v, dv;     /* the left edge's u and v, and their steps a row */
    int32_t y, rows;          /* the row, and the rows left */
    int32_t cx0, cx1, cy0, cy1;
    int32_t dudx, dvdx;
    uint32_t sw, sh; /* the picture's sides, 16.16 */
    int32_t su;      /* the row's u at its first pixel */
    Rows *r;
    RowAffine *plain, *held;
} RowsWalk;

/* clang-format off */
/* RowsWalk's places, the asm's names for them. */
#define RW_lx 0
#define RW_lxw 2
#define RW_ldx 4
#define RW_rx 8
#define RW_rxw 10
#define RW_rdx 12
#define RW_u 16
#define RW_du 20
#define RW_v 24
#define RW_dv 28
#define RW_y 32
#define RW_rows 36
#define RW_cx0 40
#define RW_cx1 44
#define RW_cy0 48
#define RW_cy1 52
#define RW_dudx 56
#define RW_dvdx 60
#define RW_sw 64
#define RW_sh 68
#define RW_su 72
#define RW_r 76
#define RW_plain 80
#define RW_held 84
#define RW_CHECK(f) static_assert(__builtin_offsetof(RowsWalk, f) == RW_##f);
RW_CHECK(lx) RW_CHECK(ldx) RW_CHECK(rx) RW_CHECK(rdx) RW_CHECK(u) RW_CHECK(du) RW_CHECK(v)
RW_CHECK(dv) RW_CHECK(y) RW_CHECK(rows) RW_CHECK(cx0) RW_CHECK(cx1) RW_CHECK(cy0) RW_CHECK(cy1)
RW_CHECK(dudx) RW_CHECK(dvdx) RW_CHECK(sw) RW_CHECK(sh) RW_CHECK(su) RW_CHECK(r) RW_CHECK(plain)
RW_CHECK(held)
#define RW_STR(n) #n
#define RW_AT(n) RW_STR(n)
/* Field f of rows_walk, operand [w]. */
#define RW(f) RW_AT(RW_##f) "+%[w]"

/* x0 in esi and x1 in ecx, as affine_walk's; from in edi and to in ecx, held in the clip: the
   row skipped, 9f, unless it shows. */
#define ROWS_RUN                                                                                   \
    "movswl " RW(lxw) ", %%esi\n\t"                                                                \
    "movw " RW(rxw) ", %%cx\n\t"                                                                   \
    "subw " RW(lxw) ", %%cx\n\t"                                                                   \
    "movswl %%cx, %%ecx\n\t"                                                                       \
    "addl %%esi, %%ecx\n\t"                                                                        \
    "cmpl %%esi, %%ecx\n\t"                                                                        \
    "jle 9f\n\t"                                                                                   \
    "movl " RW(y) ", %%eax\n\t"                                                                    \
    "cmpl " RW(cy0) ", %%eax\n\t"                                                                  \
    "jl 9f\n\t"                                                                                    \
    "cmpl " RW(cy1) ", %%eax\n\t"                                                                  \
    "jge 9f\n\t"                                                                                   \
    "movl %%esi, %%edi\n\t"                                                                        \
    "cmpl " RW(cx0) ", %%edi\n\t"                                                                  \
    "jge 2f\n\t"                                                                                   \
    "movl " RW(cx0) ", %%edi\n"                                                                    \
    "2:\n\t"                                                                                       \
    "cmpl " RW(cx1) ", %%ecx\n\t"                                                                  \
    "jle 3f\n\t"                                                                                   \
    "movl " RW(cx1) ", %%ecx\n"                                                                    \
    "3:\n\t"                                                                                       \
    "cmpl %%ecx, %%edi\n\t"                                                                        \
    "jge 9f\n\t"
/* u and v at from, ebx the prestep into the row and esi the pixels lost to the clip: su, and v in
   eax. */
#define ROWS_UV                                                                                    \
    "movzwl " RW(lx) ", %%ebx\n\t"                                                                 \
    "negl %%ebx\n\t"                                                                               \
    "addl $65536, %%ebx\n\t"                                                                       \
    "negl %%esi\n\t"                                                                               \
    "addl %%edi, %%esi\n\t"                                                                        \
    "movl " RW(dudx) ", %%eax\n\t"                                                                 \
    "imull %%ebx\n\t"                                                                              \
    "shrdl $16, %%edx, %%eax\n\t"                                                                  \
    "addl " RW(u) ", %%eax\n\t"                                                                    \
    "addl $32768, %%eax\n\t"                                                                       \
    "movl " RW(dudx) ", %%edx\n\t"                                                                 \
    "imull %%esi, %%edx\n\t"                                                                       \
    "addl %%edx, %%eax\n\t"                                                                        \
    "movl %%eax, " RW(su) "\n\t"                                                                   \
    "movl " RW(dvdx) ", %%eax\n\t"                                                                 \
    "imull %%ebx\n\t"                                                                              \
    "shrdl $16, %%edx, %%eax\n\t"                                                                  \
    "addl " RW(v) ", %%eax\n\t"                                                                    \
    "addl $32768, %%eax\n\t"                                                                       \
    "movl " RW(dvdx) ", %%edx\n\t"                                                                 \
    "imull %%esi, %%edx\n\t"                                                                       \
    "addl %%edx, %%eax\n\t"
/* The filler in ebx: plain when the walk of to - from pixels stays inside the picture. */
#define ROWS_PICK                                                                                  \
    "movl %%ecx, %%edx\n\t"                                                                        \
    "subl %%edi, %%edx\n\t"                                                                        \
    "decl %%edx\n\t"                                                                               \
    "movl " RW(held) ", %%ebx\n\t"                                                                 \
    "movl " RW(dudx) ", %%esi\n\t"                                                                 \
    "imull %%edx, %%esi\n\t"                                                                       \
    "jo 4f\n\t"                                                                                    \
    "addl " RW(su) ", %%esi\n\t"                                                                   \
    "jo 4f\n\t"                                                                                    \
    "cmpl " RW(sw) ", %%esi\n\t"                                                                   \
    "jae 4f\n\t"                                                                                   \
    "movl " RW(su) ", %%esi\n\t"                                                                   \
    "cmpl " RW(sw) ", %%esi\n\t"                                                                   \
    "jae 4f\n\t"                                                                                   \
    "movl " RW(dvdx) ", %%esi\n\t"                                                                 \
    "imull %%edx, %%esi\n\t"                                                                       \
    "jo 4f\n\t"                                                                                    \
    "addl %%eax, %%esi\n\t"                                                                        \
    "jo 4f\n\t"                                                                                    \
    "cmpl " RW(sh) ", %%esi\n\t"                                                                   \
    "jae 4f\n\t"                                                                                   \
    "cmpl " RW(sh) ", %%eax\n\t"                                                                   \
    "jae 4f\n\t"                                                                                   \
    "movl " RW(plain) ", %%ebx\n"                                                                  \
    "4:\n\t"
/* fill(r, y, from, to, u, v, 0), then the edges, u and v a row on. */
#define ROWS_FILL                                                                                  \
    "pushl $0\n\t"                                                                                 \
    "pushl %%eax\n\t"                                                                              \
    "pushl " RW(su) "\n\t"                                                                         \
    "pushl %%ecx\n\t"                                                                              \
    "pushl %%edi\n\t"                                                                              \
    "pushl " RW(y) "\n\t"                                                                          \
    "pushl " RW(r) "\n\t"                                                                          \
    "call *%%ebx\n\t"                                                                              \
    "addl $28, %%esp\n"                                                                            \
    "9:\n\t"                                                                                       \
    "movl " RW(ldx) ", %%eax\n\t"                                                                  \
    "addl %%eax, " RW(lx) "\n\t"                                                                   \
    "movl " RW(rdx) ", %%eax\n\t"                                                                  \
    "addl %%eax, " RW(rx) "\n\t"                                                                   \
    "movl " RW(du) ", %%eax\n\t"                                                                   \
    "addl %%eax, " RW(u) "\n\t"                                                                    \
    "movl " RW(dv) ", %%eax\n\t"                                                                   \
    "addl %%eax, " RW(v) "\n\t"                                                                    \
    "incl " RW(y) "\n\t"                                                                           \
    "decl " RW(rows) "\n\t"                                                                        \
    "jnz 1b"
/* clang-format on */

static RowsWalk rows_walk;

/* rows_walk's rows, one or more. */
static void rows_of(void)
{
    __asm__ volatile("1:\n\t" ROWS_RUN ROWS_UV ROWS_PICK ROWS_FILL
                     : [w] "+m"(rows_walk)
                     :
                     : "eax",
                       "ebx",
                       "ecx",
                       "edx",
                       "esi",
                       "edi",
                       "cc",
                       "memory",
                       "st",
                       "st(1)",
                       "st(2)",
                       "st(3)",
                       "st(4)",
                       "st(5)",
                       "st(6)",
                       "st(7)");
}
#endif

/* qgl$drawA: the two chains down from the topmost vertex, each row's run from the left edge's
   pixel up to the right's, its u, v and, when deep, depth at its first pixel centre, held in the
   clip; filled by plain, or held where the walk leaves the picture. */
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

    if (in_asm) {
        rows_walk.cx0 = clip.x;
        rows_walk.cx1 = clip.x + clip.w;
        rows_walk.cy0 = clip.y;
        rows_walk.cy1 = clip.y + clip.h;
        rows_walk.dudx = g->dudx;
        rows_walk.dvdx = g->dvdx;
        rows_walk.sw = (uint32_t)r->src.w << 16;
        rows_walk.sh = (uint32_t)r->src.h << 16;
        rows_walk.r = r;
        rows_walk.plain = plain;
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
#if defined(__i386__)

        if (in_asm && height > 0) {
            rows_walk.lx = l.x;
            rows_walk.ldx = l.dx;
            rows_walk.rx = rt.x;
            rows_walk.rdx = rt.dx;
            rows_walk.u = c.u;
            rows_walk.du = c.du;
            rows_walk.v = c.v;
            rows_walk.dv = c.dv;
            rows_walk.y = y;
            rows_walk.rows = height;
            rows_of();
            l.x = rows_walk.lx;
            rt.x = rows_walk.rx;
            c.u = rows_walk.u;
            c.v = rows_walk.v;
            y = (int16_t)rows_walk.y;
            continue;
        }
#endif

        for (int16_t row = 0; row < height; row++, y++) {
            const int32_t into = prestep(l.x), x0 = (int16_t)FXFLOOR(l.x);
            const int32_t x1 = x0 + (int16_t)(FXFLOOR(rt.x) - FXFLOOR(l.x));
            const int32_t from = rect_max(x0, clip.x), to = rect_min(x1, clip.x + clip.w);

            if (x1 > x0 && y >= clip.y && y < clip.y + clip.h && from < to) {
                const int32_t lost = from - x0;
                const int32_t u = add32(add32(c.u, 32768), FXMUL(into, g->dudx));
                const int32_t vv = add32(add32(c.v, 32768), FXMUL(into, g->dvdx));
                const int32_t z = deep ? scan_fistp_low((double)c.z * DEPTH_NEAR * 65536.0) : 0;
                const Steps st = {
                    (int32_t)(u + (int64_t)lost * g->dudx),
                    (int32_t)(vv + (int64_t)lost * g->dvdx),
                    g->dudx,
                    g->dvdx,
                    0,
                };

                RowAffine *const fill = steps_inside(&st, &r->src, to - from) ? plain : held;

                fill(r, y, from, to, st.u, st.v, deep ? depth_on(z, g->dzdx, lost) : 0);
            }

            l.x = add32(l.x, l.dx);
            rt.x = add32(rt.x, rt.dx);
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
            const int32_t from = rect_max(x0, clip.x), to = rect_min(x1, clip.x + clip.w);

            if (x1 > x0 && y >= clip.y && y < clip.y + clip.h && from < to) {
                const double diff = (double)into * r65536, lost = from - x0;
                const int32_t z = scan_fistp_low((double)c.z * DEPTH_NEAR * 65536.0);
                const double pu = (double)c.pu + (double)g->fdudx * diff;
                const double pv = (double)c.pv + (double)g->fdvdx * diff;
                const double pz = (double)c.z + (double)g->fdzdx * diff;

                fill(
                    r,
                    y,
                    from,
                    to,
                    pu + g->fdudx * lost,
                    pv + g->fdvdx * lost,
                    pz + g->fdzdx * lost,
                    depth_on(z, g->dzdx, from - x0)
                );
            }

            l.x = add32(l.x, l.dx);
            rt.x = add32(rt.x, rt.dx);
            carried_on(&c, true, true);
        }
    }
}
