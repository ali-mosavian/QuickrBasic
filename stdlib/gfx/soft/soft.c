/* The software backend (soft.h). */
#include "soft/soft.h"
#include "gfx_qx.h"
#include "base/disc.h"
#include "base/gmath.h"
#include "soft/persp.h"
#include "soft/scanner.h"
#include "base/cover.h"

static uint8_t *at(const SoftTarget *const t, const int32_t x, const int32_t y)
{
    return t->px.p + y * t->px.pitch + x * px_bytes(t->px.format);
}

/* The ink has nothing to do with s's texels: the fused fillers draw them. */
static bool fused(const SoftTarget *const t, const Pixels *const s)
{
    return !t->ink.effects && s->format == t->px.format;
}

static bool blends(const Ink *const k) { return k->blend || k->mode; }

/* The walk n texels on. */
static Steps advanced(Steps st, const int32_t n)
{
    const int64_t u = st.u + (int64_t)st.du * n;
    st.u = st.wrap ? fill_wrap((int32_t)(u % st.wrap), st.wrap) : (int32_t)u;
    st.v = (int32_t)(st.v + (int64_t)st.dv * n);
    return st;
}

/* n pixels from x, y through the call's ink: s's texels along the walk, or colour when s is null
   (drawn even when 0). */
static void inked(
    const SoftTarget *const t,
    const int32_t x,
    const int32_t y,
    const int32_t n,
    const Pixels *const s,
    Steps st,
    const uint32_t colour
)
{
    uint32_t texels[FILL_CHUNK];
    Ink k = t->ink;
    k.keyed = k.keyed && s;
    k.source = s ? s->format : k.source;
    const uint32_t bytes = px_bytes(t->px.format);

    for (int32_t done = 0; done < n; done += FILL_CHUNK) {
        const int32_t m = rect_min(n - done, FILL_CHUNK);

        if (s) {
            fill_formats[s->format]->fetch(texels, s, st, m);
            st = advanced(st, m);
        } else
            for (int32_t i = 0; i < m; i++)
                texels[i] = colour;

        t->fill->ink(at(t, x, y) + done * bytes, texels, m, &k);
    }
}

/* Row y from x0 up to x1 in colour, clipped. */
static void span(
    const SoftTarget *const t,
    const int32_t y,
    const int32_t x0,
    const int32_t x1,
    const uint32_t colour
)
{
    const int32_t from = rect_max(x0, t->clip.x), to = rect_min(x1, t->clip.x + t->clip.w);

    if (y < t->clip.y || y >= t->clip.y + t->clip.h || from >= to)
        return;

    if (blends(&t->ink))
        inked(t, from, y, to - from, nullptr, (Steps){}, colour);
    else
        t->fill->span(at(t, from, y), to - from, t->fill->paint(colour, &t->ink));
}

/* s's texels along the walk into n pixels from x, y: the fused filler, or through the ink. */
static void texels(
    const SoftTarget *const t,
    const int32_t x,
    const int32_t y,
    const int32_t n,
    const Pixels *const s,
    const Steps st,
    FillStep *const fill
)
{
    if (fused(t, s))
        fill(at(t, x, y), s, st, n);
    else
        inked(t, x, y, n, s, st, 0);
}

void soft_clear(const SoftTarget *const t, const uint32_t colour)
{
    for (int32_t y = t->clip.y; y < t->clip.y + t->clip.h; y++)
        span(t, y, t->clip.x, t->clip.x + t->clip.w, colour);
}

void soft_pset(const SoftTarget *const t, const float x, const float y, const uint32_t colour)
{
    const int32_t px = cover_pixel(x), py = cover_pixel(y);
    span(t, py, px, px + 1, colour);
}

/* The part of the line from a to b inside r widened by a pixel, Liang and Barsky's, in doubles: a
   float's 24 bits lose a far end's place. False: none. */
static bool cut(Vec *const a, Vec *const b, const Rect r)
{
    const double ax = a->x, ay = a->y, dx = (double)b->x - ax, dy = (double)b->y - ay;
    const double p[] = {-dx, dx, -dy, dy};

    const double q[] = {ax - (r.x - 1), (r.x + r.w) - ax, ay - (r.y - 1), (r.y + r.h) - ay};

    double from = 0, to = 1;

    for (uint32_t i = 0; i < 4; i++) {
        if (p[i] == 0 && q[i] < 0)
            return false;

        if (p[i] < 0)
            from = q[i] / p[i] > from ? q[i] / p[i] : from;
        else if (p[i] > 0)
            to = q[i] / p[i] < to ? q[i] / p[i] : to;
    }

    *a = (Vec){(float)(ax + from * dx), (float)(ay + from * dy)};

    *b = (Vec){(float)(ax + to * dx), (float)(ay + to * dy)};

    return from <= to;
}

/* Bresenham's, from pixel to pixel, the end drawn when last. A line longer than LONG is cut to the
   clip first: its pixels in it may then fall a rounding apart. */
static void walk(const SoftTarget *const t, Vec a, Vec b, const uint32_t colour, const bool last)
{
    const float LONG = 1 << 15;

    if ((b.x - a.x > LONG || a.x - b.x > LONG || b.y - a.y > LONG || a.y - b.y > LONG) &&
        !cut(&a, &b, t->clip))
        return;

    int32_t x = cover_pixel(a.x), y = cover_pixel(a.y);
    const int32_t to_x = cover_pixel(b.x), to_y = cover_pixel(b.y);
    const int32_t dx = to_x > x ? to_x - x : x - to_x, dy = -(to_y > y ? to_y - y : y - to_y);
    const int32_t sx = x < to_x ? 1 : -1, sy = y < to_y ? 1 : -1;

    for (int32_t err = dx + dy;;) {
        const bool end = x == to_x && y == to_y;

        if (!end || last)
            span(t, y, x, x + 1, colour);

        if (end)
            return;

        const int32_t e2 = 2 * err;

        if (e2 >= dy) {
            err += dy;
            x += sx;
        }

        if (e2 <= dx) {
            err += dx;
            y += sy;
        }
    }
}

void soft_line(const SoftTarget *const t, const Vec a, const Vec b, const uint32_t colour)
{
    walk(t, a, b, colour, true);
}

void soft_outline(
    const SoftTarget *const t,
    const Vec *const p,
    const uint32_t n,
    const uint32_t colour
)
{
    for (uint32_t i = 0; i < n; i++)
        walk(t, p[i], p[(i + 1) % n], colour, false);
}

/* gfx's corner as qgl's: qgl's pixel i spans [i, i + 1) and its texel j is centred on j, where
   gfx centres pixel i on i and texel j on j + 0.5. */
static ScanVert qgl_corner(const float x, const float y, const float u, const float v)
{
    return (ScanVert){x + 0.5f, y + 0.5f, 1, u - 0.5f, v - 0.5f};
}

/* Row y's pixels within r of c (disc.h), [*x0, *x1): false, none. The square root's ends are the
   rule's unless one rounds near a pixel's centre: near bounds its rounding sixteen times over
   where the row reaches r / 8 each side of c; there, and on narrower rows, disc_within settles
   them. */
static bool chord(const Vec c, const float r, const int32_t y, int32_t *const x0, int32_t *const x1)
{
    if (!(r > 0))
        return false;

    const float dy = (float)y - c.y, h2 = r * r - dy * dy, h = h2 > 0 ? gm_sqrt(h2) : 0;
    const float near = 0x1p-18f * (9 * r + (c.x < 0 ? -c.x : c.x) + 1);
    int32_t a = cover_first(c.x - h), b = cover_first(c.x + h);
    const float left = (float)a - (c.x - h), right = (float)b - (c.x + h);

    if (!(h >= r / 8 && left > near && left < 1 - near && right > near && right < 1 - near)) {
        while (disc_within((float)(a - 1) - c.x, dy, r))
            a--;

        while (a < b && !disc_within((float)a - c.x, dy, r))
            a++;

        while (disc_within((float)b - c.x, dy, r))
            b++;

        while (b > a && !disc_within((float)(b - 1) - c.x, dy, r))
            b--;
    }

    *x0 = a;
    *x1 = b;
    return a < b;
}

/* A part of a circle's row, x0 up to x1. */
typedef void Piece(const SoftTarget *t, int32_t y, int32_t x0, int32_t x1, const void *ctx);

/* The pixels k covers, its rows' parts to piece. */
static void ring(
    const SoftTarget *const t,
    const Disc *const k,
    Piece *const piece,
    const void *const ctx
)
{
    const int32_t y0 = rect_max(cover_first(k->c.y - k->outer) - 1, t->clip.y);
    const int32_t y1 = rect_min(cover_first(k->c.y + k->outer) + 1, t->clip.y + t->clip.h);

    for (int32_t y = y0; y < y1; y++) {
        int32_t a0, a1, b0, b1;

        if (!chord(k->c, k->outer, y, &a0, &a1))
            continue;

        if (!chord(k->c, k->inner, y, &b0, &b1)) {
            piece(t, y, a0, a1, ctx);
            continue;
        }

        piece(t, y, a0, b0, ctx);
        piece(t, y, b1, a1, ctx);
    }
}

static void ring_span(
    const SoftTarget *const t,
    const int32_t y,
    const int32_t x0,
    const int32_t x1,
    const void *const ctx
)
{
    span(t, y, x0, x1, *(const uint32_t *)ctx);
}

void soft_circle(
    const SoftTarget *const t,
    const Vec c,
    const float r,
    const uint32_t colour,
    const bool fill
)
{
    const Disc k = disc_of(c, r, fill);
    ring(t, &k, ring_span, &colour);
}

/* An arc's: its disc, sweep and colour. */
typedef struct {
    Disc k;
    Sweep w;
    uint32_t colour;
} Arc;

static void arc_piece(
    const SoftTarget *const t,
    const int32_t y,
    const int32_t x0,
    const int32_t x1,
    const void *const ctx
)
{
    const Arc *const a = ctx;

    for (int32_t x = x0; x < x1; x++)
        if (swept(&a->w, (float)x - a->k.c.x, (float)y - a->k.c.y))
            span(t, y, x, x + 1, a->colour);
}

void soft_arc(
    const SoftTarget *const t,
    const Vec c,
    const float r,
    const float start,
    const float finish,
    const uint32_t colour
)
{
    const Arc a = {disc_of(c, r, false), sweep_of(start, finish), colour};

    ring(t, &a.k, arc_piece, &a);
}

void soft_rect(
    const SoftTarget *const t,
    const float x1,
    const float y1,
    const float x2,
    const float y2,
    const uint32_t colour,
    const bool fill
)
{
    const float l = (x1 < x2 ? x1 : x2) - 0.5f, r = (x1 < x2 ? x2 : x1) + 0.5f;
    const float top = (y1 < y2 ? y1 : y2) - 0.5f, bottom = (y1 < y2 ? y2 : y1) + 0.5f;

    const int32_t x0 = cover_first(l), x1_ = cover_first(r), y0 = cover_first(top),
                  y1_ = cover_first(bottom);

    /* scan_poly's pixels for these corners, without its edges at every row */
    if (fill) {
        for (int32_t y = rect_max(y0, t->clip.y); y < rect_min(y1_, t->clip.y + t->clip.h); y++)
            span(t, y, x0, x1_, colour);

        return;
    }

    if (x0 >= x1_ || y0 >= y1_)
        return;

    span(t, y0, x0, x1_, colour);

    if (y1_ - 1 > y0)
        span(t, y1_ - 1, x0, x1_, colour);

    for (int32_t y = y0 + 1; y < y1_ - 1; y++) {
        span(t, y, x0, x0 + 1, colour);

        if (x1_ - 1 > x0)
            span(t, y, x1_ - 1, x1_, colour);
    }
}

/* A draw call's flags as a row filler's variant. */
static uint32_t variant(const int32_t flags)
{
    return (flags & GFX_DRAW_FLIPX ? ROW_FLIPX : 0) | (flags & GFX_PEN_OPAQUE ? ROW_OPAQUE : 0);
}

void soft_sprite(
    const SoftTarget *const t,
    const Pixels *const s,
    const float x,
    const float y,
    const int32_t flags
)
{
    const int32_t left = cover_pixel(x), top = cover_pixel(y);
    const Rect drawn = rect_meet((Rect){left, top, s->w, s->h}, t->clip);

    FillRow *const row = t->fill->row[variant(flags)];
    const bool flip_x = flags & GFX_DRAW_FLIPX, flip_y = flags & GFX_DRAW_FLIPY;
    const int32_t u = flip_x ? s->w - 1 - (drawn.x - left) : drawn.x - left;
    const bool plain = fused(t, s);

    for (int32_t y_ = drawn.y; y_ < drawn.y + drawn.h; y_++) {
        const int32_t v = flip_y ? s->h - 1 - (y_ - top) : y_ - top;
        const Steps st = {u << 16, v << 16, flip_x ? -65536 : 65536, 0, 0};

        if (plain)
            row(at(t, drawn.x, y_), s->p + v * s->pitch + u * px_bytes(s->format), drawn.w);
        else
            inked(t, drawn.x, y_, drawn.w, s, st, 0);
    }
}

typedef struct {
    const SoftTarget *t;
    const Pixels *s;
    FillStep *step;
    bool wrap, opaque;
    ScanMode mode;
    bool test, set;
} Textured;

/* f held inside [0, most) texels, 16.16 */
static int32_t held(const int64_t f, const int32_t most)
{
    const int32_t top = (most << 16) - 1;
    return f <= 0 ? 0 : f >= top ? top : (int32_t)f;
}

/* The walk over n pixels from the first, its texels inside the surface: its ends held in, so
   every texel between is too. */
static Steps inside_steps(
    const int32_t u,
    const int32_t v,
    const int32_t du,
    const int32_t dv,
    const Pixels *const s,
    const int32_t n
)
{
    const int32_t u0 = held(u, s->w), v0 = held(v, s->h);
    const int32_t u1 = held(u + (int64_t)du * (n - 1), s->w);
    const int32_t v1 = held(v + (int64_t)dv * (n - 1), s->h);

    return n > 1 ? (Steps){u0, v0, (u1 - u0) / (n - 1), (v1 - v0) / (n - 1), 0}

                 : (Steps){u0, v0, 0, 0, 0};
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

/* n pixels' walk along u, v and their steps (16.16): wrapping in u, or inside s. */
static Steps walk_of(
    const Textured *const x,
    const int32_t n,
    const int32_t u,
    const int32_t v,
    const int32_t du,
    const int32_t dv
)
{
    Steps st = inside_steps(u, v, du, dv, x->s, n);

    if (x->wrap) {
        const int64_t wrap = (int64_t)x->s->w << 16;
        st.wrap = (int32_t)wrap;
        st.u = (int32_t)(((int64_t)u % wrap + wrap) % wrap);
        st.du = du;
    }

    return st;
}

/* n pixels from x0, y of the span's texels, as textured() walks them, each shown over the depth
   there and its depth set as asked: a keyed texel draws nothing. */
static void deep(
    const Textured *const x,
    const int32_t x0,
    const int32_t y,
    const int32_t n,
    int32_t u,
    int32_t v,
    const ScanSpan *const sp,
    int32_t z
)
{
    const SoftTarget *const t = x->t;
    const Pixels *const s = x->s;
    const uint32_t bytes = px_bytes(s->format);
    const bool plain = fused(t, s);
    uint16_t *const d = t->depth.p + y * t->depth.row + x0;
    Steps st = walk_of(x, n, u, v, sp->du, sp->dv);
    Ink k = t->ink;
    k.keyed = false;
    k.source = s->format;

    for (int32_t i = 0; i < n; i++, z = depth_on(z, sp->dz, 1)) {
        uint32_t texel;
        const uint16_t here = depth_of(z);
        uint8_t *const out = at(t, x0 + i, y);

        fill_formats[s->format]->fetch(&texel, s, st, 1);
        st = advanced(st, 1);

        if (x->test && !depth_shows(d[i], here))
            continue;

        if (!x->opaque && px_key(s->format, texel))
            continue;

        if (plain)
            px_store(out, texel, bytes);
        else
            t->fill->ink(out, &texel, 1, &k);

        if (x->set)
            d[i] = here;
    }
}

static void textured(void *const ctx, const ScanSpan *const sp)
{
    const Textured *const x = ctx;
    int32_t x0, x1;

    if (!held_span(x->t, sp, &x0, &x1))
        return;

    const int64_t lost = x0 - sp->x0;
    const int32_t u = (int32_t)(sp->u + lost * sp->du), v = (int32_t)(sp->v + lost * sp->dv);
    const int32_t n = x1 - x0;

    if (x->t->depth.p)
        deep(x, x0, sp->y, n, u, v, sp, depth_on(sp->z, sp->dz, (int32_t)lost));
    else
        texels(x->t, x0, sp->y, n, x->s, walk_of(x, n, u, v, sp->du, sp->dv), x->step);
}

/* a perspective span: persp.h walks it */
static void perspective(void *const ctx, const ScanSpan *const sp)
{
    const Textured *const x = ctx;
    const SoftTarget *const t = x->t;
    ScanSpan c = *sp;
    int32_t x0, x1;

    if (!held_span(t, sp, &x0, &x1))
        return;

    const double lost = x0 - sp->x0;
    c.x0 = x0;
    c.x1 = x1;
    c.pu += sp->dpu * lost;
    c.pv += sp->dpv * lost;
    c.pz += sp->dpz * lost;
    c.z = depth_on(sp->z, sp->dz, x0 - sp->x0);

    Ink k = t->ink;
    k.keyed = false;
    k.source = x->s->format;

    persp_span(
        &(PerspDraw){
            .d = at(t, x0, sp->y),
            .bytes = px_bytes(t->px.format),
            .s = *x->s,
            .opaque = x->opaque,
            .wrap = x->wrap,
            .whole = x->mode == SCAN_ATEX,
            .ink = fused(t, x->s) ? nullptr : &k,
            .put = t->fill->ink,
            .depth = t->depth.p ? t->depth.p + sp->y * t->depth.row + x0 : nullptr,
            .test = x->test,
            .set = x->set,
        },
        &c
    );
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
    const bool persp = mode == SCAN_PTEX || mode == SCAN_ATEX;
    const bool opaque = flags & GFX_PEN_OPAQUE;
    Textured x = {
        .t = t,
        .s = s,
        .step = t->fill->step[opaque ? STEP_OPAQUE : 0],
        .wrap = wrap,
        .opaque = opaque,
        .mode = mode,
        .test = flags & GFX_DRAW_ZTEST,
        .set = flags & GFX_DRAW_ZSET,
    };

    scan_poly(v, n, mode, t->clip, gfx, persp ? perspective : textured, &x);
}

void soft_texture_persp(
    const SoftTarget *const t,
    const Pixels *const s,
    const ScanVert *const v,
    const uint32_t n,
    const int32_t flags,
    const bool wrap,
    const ScanMode mode
)
{
    texture(t, s, v, n, flags, wrap, mode, false);
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
        span(t, sp->y, sp->x0, sp->x1, f->colour);
        return;
    }

    if (!held_span(t, sp, &x0, &x1))
        return;

    uint16_t *const d = t->depth.p + sp->y * t->depth.row;
    int32_t z = depth_on(sp->z, sp->dz, x0 - sp->x0), from = x0;

    for (int32_t x = x0; x < x1; x++, z = depth_on(z, sp->dz, 1)) {
        const uint16_t here = depth_of(z);

        if (f->test && !depth_shows(d[x], here)) {
            span(t, sp->y, from, x, f->colour);
            from = x + 1;
        } else if (f->set)
            d[x] = here;
    }

    span(t, sp->y, from, x1, f->colour);
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
