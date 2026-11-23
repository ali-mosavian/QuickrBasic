/* The software backend (soft.h). */
#include "soft/soft.h"
#include "gfx_qx.h"
#include "gmath.h"
#include "soft/scan.h"

enum { GLYPH_W = 8 };

static uint8_t *at(const SoftTarget *const t, const int32_t x, const int32_t y)
{
    return t->px.p + y * t->px.pitch + x;
}

static bool inside(const Rect r, const int32_t x, const int32_t y)
{
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
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

    if (y >= t->clip.y && y < t->clip.y + t->clip.h && from < to)
        t->fill->span(at(t, from, y), to - from, colour);
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

        if ((!end || last) && inside(t->clip, x, y))
            t->fill->span(at(t, x, y), 1, colour);

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

/* Row y's pixels whose centres lie within rr of c, [*x0, *x1): false, none. */
static bool chord(
    const Vec c,
    const float rr,
    const int32_t y,
    int32_t *const x0,
    int32_t *const x1
)
{
    const float dy = (float)y - c.y, h2 = rr * rr - dy * dy;

    if (rr <= 0 || h2 < 0)
        return false;

    const float h = gm_sqrt(h2);
    *x0 = scan_first(c.x - h);
    *x1 = scan_first(c.x + h);
    return *x0 < *x1;
}

/* A part of a circle's row, x0 up to x1. */
typedef void Piece(const SoftTarget *t, int32_t y, int32_t x0, int32_t x1, const void *ctx);

/* The circle about c of radius r, its rows' parts to piece: filled, the pixels within r and a
   half; else the outline, those from r less a half. */
static void ring(
    const SoftTarget *const t,
    const Vec c,
    const float r,
    const bool fill,
    Piece *const piece,
    const void *const ctx
)
{
    const float outer = r + 0.5f, inner = fill ? 0 : r - 0.5f;
    const int32_t y0 = rect_max(scan_first(c.y - outer), t->clip.y);
    const int32_t y1 = rect_min(scan_first(c.y + outer), t->clip.y + t->clip.h);

    for (int32_t y = y0; y < y1; y++) {
        int32_t a0, a1, b0, b1;

        if (!chord(c, outer, y, &a0, &a1))
            continue;

        if (!chord(c, inner, y, &b0, &b1)) {
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
    ring(t, c, r, fill, ring_span, &colour);
}

/* An arc's sweep: from direction s clockwise to f; wide, past half a turn. */
typedef struct {
    Vec c, s, f;
    bool wide, whole;
    uint32_t colour;
} Sweep;

/* d, from the centre, lies in the sweep: within half a turn, clockwise of s and anticlockwise of
   f; past it, not strictly in the rest. */
static bool swept(const Sweep *const w, const float dx, const float dy)
{
    const float from = w->s.x * dy - w->s.y * dx, to = dx * w->f.y - dy * w->f.x;
    return w->whole || (w->wide ? !(from < 0 && to < 0) : from >= 0 && to >= 0);
}

static void arc_piece(
    const SoftTarget *const t,
    const int32_t y,
    const int32_t x0,
    const int32_t x1,
    const void *const ctx
)
{
    const Sweep *const w = ctx;

    for (int32_t x = x0; x < x1; x++)
        if (swept(w, (float)x - w->c.x, (float)y - w->c.y))
            span(t, y, x, x + 1, w->colour);
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
    const float turn = 2 * GM_PI, sweep = finish - start;
    Sweep w = {
        .c = c,
        .wide = sweep - (float)gm_floor(sweep / turn) * turn > GM_PI,
        .whole = sweep >= turn,
        .colour = colour,
    };

    gm_sincos(start, &w.s.y, &w.s.x);
    gm_sincos(finish, &w.f.y, &w.f.x);
    ring(t, c, r, false, arc_piece, &w);
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

    for (int32_t y_ = drawn.y; y_ < drawn.y + drawn.h; y_++) {
        const int32_t v = flip_y ? s->h - 1 - (y_ - top) : y_ - top;
        row(at(t, drawn.x, y_), s->p + v * s->pitch + u, drawn.w);
    }
}

void soft_glyph(
    const SoftTarget *const t,
    const uint8_t *const rows,
    const int32_t h,
    const float x,
    const float y,
    const uint32_t colour
)
{
    const int32_t left = scan_pixel(x), top = scan_pixel(y);

    for (int32_t r = 0; r < h; r++)
        for (int32_t c = 0; c < GLYPH_W; c++)
            if (rows[r] & 0x80 >> c && inside(t->clip, left + c, top + r))
                t->fill->span(at(t, left + c, top + r), 1, colour);
}
