/* The software backend (soft.h). */
#include "soft/soft.h"
#include "gfx_qx.h"
#include "disc.h"
#include "gmath.h"
#include "soft/scan.h"

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

typedef struct {
    const SoftTarget *t;
    uint32_t colour;
} Flat;

static void flat(void *const ctx, const int32_t y, const int32_t x0, const int32_t x1)
{
    const Flat *const f = ctx;
    span(f->t, y, x0, x1, f->colour);
}

void soft_clear(const SoftTarget *const t, const uint32_t colour)
{
    for (int32_t y = t->clip.y; y < t->clip.y + t->clip.h; y++)
        span(t, y, t->clip.x, t->clip.x + t->clip.w, colour);
}

void soft_pset(const SoftTarget *const t, const float x, const float y, const uint32_t colour)
{
    const int32_t px = scan_pixel(x), py = scan_pixel(y);
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

    int32_t x = scan_pixel(a.x), y = scan_pixel(a.y);
    const int32_t to_x = scan_pixel(b.x), to_y = scan_pixel(b.y);
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

void soft_poly(
    const SoftTarget *const t,
    const Vec *const p,
    const uint32_t n,
    const uint32_t colour
)
{
    scan_convex(p, n, t->clip, flat, &(Flat){t, colour});
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
    int32_t a = scan_first(c.x - h), b = scan_first(c.x + h);
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
    const int32_t y0 = rect_max(scan_first(k->c.y - k->outer) - 1, t->clip.y);
    const int32_t y1 = rect_min(scan_first(k->c.y + k->outer) + 1, t->clip.y + t->clip.h);

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

    if (fill) {
        const Vec corners[] = {{l, top}, {r, top}, {r, bottom}, {l, bottom}};

        soft_poly(t, corners, 4, colour);
        return;
    }

    const int32_t x0 = scan_first(l), x1_ = scan_first(r), y0 = scan_first(top),
                  y1_ = scan_first(bottom);

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
    const int32_t left = scan_pixel(x), top = scan_pixel(y);
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

/* The texels' map over a polygon: at a pixel's centre (x, y), u = ux x + uy y + u0, v likewise. */
typedef struct {
    float ux, uy, u0, vx, vy, v0;
} Map;

/* The map through three corners: false, they lie on a line. */
static bool map_of(const Vertex *const p, Map *const m)
{
    const float dx1 = p[1].x - p[0].x, dy1 = p[1].y - p[0].y;
    const float dx2 = p[2].x - p[0].x, dy2 = p[2].y - p[0].y;
    const float det = dx1 * dy2 - dx2 * dy1;

    if (det > -1e-6f && det < 1e-6f)
        return false;

    const float du1 = p[1].u - p[0].u, du2 = p[2].u - p[0].u;
    const float dv1 = p[1].v - p[0].v, dv2 = p[2].v - p[0].v;
    m->ux = (du1 * dy2 - du2 * dy1) / det;
    m->uy = (du2 * dx1 - du1 * dx2) / det;
    m->vx = (dv1 * dy2 - dv2 * dy1) / det;
    m->vy = (dv2 * dx1 - dv1 * dx2) / det;
    m->u0 = p[0].u - m->ux * p[0].x - m->uy * p[0].y;
    m->v0 = p[0].v - m->vx * p[0].x - m->vy * p[0].y;

    return true;
}

typedef struct {
    const SoftTarget *t;
    const Pixels *s;
    Map m;
    FillStep *step;
    bool wrap;
} Textured;

static int32_t fixed(const float f) { return (int32_t)(f * 65536); }

/* f in 16.16, inside [0, most). */
static int32_t fixed_in(const float f, const int32_t most)
{
    const int32_t top = (most << 16) - 1;
    return f <= 0 ? 0 : f * 65536 >= (float)top ? top : fixed(f);
}

/* The walk over n pixels from the first, its texels inside the surface: its ends held in, so
   every texel between is too. */
static Steps inside_steps(
    const float u,
    const float v,
    const float du,
    const float dv,
    const Pixels *const s,
    const int32_t n
)
{
    const int32_t u0 = fixed_in(u, s->w), v0 = fixed_in(v, s->h);
    const int32_t u1 = fixed_in(u + du * (float)(n - 1), s->w);
    const int32_t v1 = fixed_in(v + dv * (float)(n - 1), s->h);

    return n > 1 ? (Steps){u0, v0, (u1 - u0) / (n - 1), (v1 - v0) / (n - 1), 0}

                 : (Steps){u0, v0, 0, 0, 0};
}

static void textured(void *const ctx, const int32_t y, const int32_t x0, const int32_t x1)
{
    const Textured *const x = ctx;
    const Map *const m = &x->m;
    const float u = m->ux * (float)x0 + m->uy * (float)y + m->u0;
    const float v = m->vx * (float)x0 + m->vy * (float)y + m->v0;
    const int32_t n = x1 - x0;
    Steps st = inside_steps(u, v, m->ux, m->vx, x->s, n);

    if (x->wrap) {
        const float w = (float)x->s->w;
        st.wrap = x->s->w << 16;
        st.u = fill_wrap(fixed(u - (float)gm_floor(u / w) * w), st.wrap);
        st.du = fixed(m->ux);
    }

    texels(x->t, x0, y, n, x->s, st, x->step);
}

void soft_texture(
    const SoftTarget *const t,
    const Pixels *const s,
    const Vertex *const v,
    const uint32_t n,
    const int32_t flags,
    const bool wrap
)
{
    Textured x = {
        .t = t,
        .s = s,
        .step = t->fill->step[flags & GFX_PEN_OPAQUE ? STEP_OPAQUE : 0],
        .wrap = wrap,
    };

    if (!map_of(v, &x.m))
        return;

    Vec p[4];

    for (uint32_t i = 0; i < n; i++)
        p[i] = (Vec){v[i].x, v[i].y};

    scan_convex(p, n, t->clip, textured, &x);
}
