/* Shapes (soft.h): points, lines, rectangles, circles and arcs as flat spans. */
#include "base/cover.h"
#include "base/disc.h"
#include "base/gmath.h"
#include "backend/soft/span.h"

void soft_clear(const SoftTarget *const t, const uint32_t colour)
{
    const Flat f = span_flat_of(t, colour);

    if (t->clip.w <= 0)
        return;

    for (int32_t y = t->clip.y; y < t->clip.y + t->clip.h; y++)
        f.fill(&f.r, y, t->clip.x, t->clip.x + t->clip.w, 0);
}

void soft_pset(const SoftTarget *const t, const float x, const float y, const uint32_t colour)
{
    const int32_t px = cover_pixel(x), py = cover_pixel(y);
    const Flat f = span_flat_of(t, colour);

    span_flat(t, &f, py, px, px + 1);
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
static void walk(const SoftTarget *const t, const Flat *const f, Vec a, Vec b, const bool last)
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
            span_flat(t, f, y, x, x + 1);

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
    const Flat f = span_flat_of(t, colour);

    walk(t, &f, a, b, true);
}

void soft_outline(
    const SoftTarget *const t,
    const Vec *const p,
    const uint32_t n,
    const uint32_t colour
)
{
    const Flat f = span_flat_of(t, colour);

    for (uint32_t i = 0; i < n; i++)
        walk(t, &f, p[i], p[(i + 1) % n], false);
}

/* A circle's or an arc's: its target and filler. */
typedef struct {
    const SoftTarget *t;
    Flat f;
} Ring;

static void ring_span(void *const ctx, const int32_t y, const int32_t x0, const int32_t x1)
{
    const Ring *const r = ctx;
    span_flat(r->t, &r->f, y, x0, x1);
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
    Ring ring = {t, span_flat_of(t, colour)};

    disc_runs(&k, nullptr, t->clip.y, t->clip.y + t->clip.h, ring_span, &ring);
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
    const Disc k = disc_of(c, r, false);
    const Sweep w = sweep_of(start, finish);
    Ring ring = {t, span_flat_of(t, colour)};

    disc_runs(&k, &w, t->clip.y, t->clip.y + t->clip.h, ring_span, &ring);
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

    const Flat f = span_flat_of(t, colour);

    /* scan_flat's pixels for these corners, without its edges at every row */
    if (fill) {
        for (int32_t y = rect_max(y0, t->clip.y); y < rect_min(y1_, t->clip.y + t->clip.h); y++)
            span_flat(t, &f, y, x0, x1_);

        return;
    }

    if (x0 >= x1_ || y0 >= y1_)
        return;

    span_flat(t, &f, y0, x0, x1_);

    if (y1_ - 1 > y0)
        span_flat(t, &f, y1_ - 1, x0, x1_);

    for (int32_t y = y0 + 1; y < y1_ - 1; y++) {
        span_flat(t, &f, y, x0, x0 + 1);

        if (x1_ - 1 > x0)
            span_flat(t, &f, y, x1_ - 1, x1_);
    }
}
