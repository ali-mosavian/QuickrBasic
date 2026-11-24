/* Shapes (soft.h): points, lines, rectangles, circles and arcs as flat spans. */
#include "base/cover.h"
#include "base/disc.h"
#include "base/gmath.h"
#include "backend/soft/span.h"

void soft_clear(const SoftTarget *const t, const uint32_t colour)
{
    for (int32_t y = t->clip.y; y < t->clip.y + t->clip.h; y++)
        span_flat(t, y, t->clip.x, t->clip.x + t->clip.w, colour);
}

void soft_pset(const SoftTarget *const t, const float x, const float y, const uint32_t colour)
{
    const int32_t px = cover_pixel(x), py = cover_pixel(y);
    span_flat(t, py, px, px + 1, colour);
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
            span_flat(t, y, x, x + 1, colour);

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
    span_flat(t, y, x0, x1, *(const uint32_t *)ctx);
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
            span_flat(t, y, x, x + 1, a->colour);
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
            span_flat(t, y, x0, x1_, colour);

        return;
    }

    if (x0 >= x1_ || y0 >= y1_)
        return;

    span_flat(t, y0, x0, x1_, colour);

    if (y1_ - 1 > y0)
        span_flat(t, y1_ - 1, x0, x1_, colour);

    for (int32_t y = y0 + 1; y < y1_ - 1; y++) {
        span_flat(t, y, x0, x0 + 1, colour);

        if (x1_ - 1 > x0)
            span_flat(t, y, x1_ - 1, x1_, colour);
    }
}
