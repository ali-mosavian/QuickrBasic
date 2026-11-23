/* The software backend (soft.h). */
#include "soft/soft.h"
#include "gfx_qx.h"
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
static bool cut(ScanPoint *const a, ScanPoint *const b, const Rect r)
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

    *a = (ScanPoint){(float)(ax + from * dx), (float)(ay + from * dy)};

    *b = (ScanPoint){(float)(ax + to * dx), (float)(ay + to * dy)};

    return from <= to;
}

/* Bresenham's, from pixel to pixel. A line longer than LONG is cut to the clip first: its pixels
   in it may then fall a rounding apart. */
void soft_line(
    const SoftTarget *const t,
    const float x1,
    const float y1,
    const float x2,
    const float y2,
    const uint32_t colour
)
{
    const float LONG = 1 << 15;
    ScanPoint a = {x1, y1}, b = {x2, y2};

    if ((x2 - x1 > LONG || x1 - x2 > LONG || y2 - y1 > LONG || y1 - y2 > LONG) &&
        !cut(&a, &b, t->clip))
        return;

    int32_t x = scan_pixel(a.x), y = scan_pixel(a.y);
    const int32_t to_x = scan_pixel(b.x), to_y = scan_pixel(b.y);
    const int32_t dx = to_x > x ? to_x - x : x - to_x, dy = -(to_y > y ? to_y - y : y - to_y);
    const int32_t sx = x < to_x ? 1 : -1, sy = y < to_y ? 1 : -1;

    for (int32_t err = dx + dy;;) {
        if (inside(t->clip, x, y))
            t->fill->span(at(t, x, y), 1, colour);

        if (x == to_x && y == to_y)
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
        const ScanPoint corners[] = {{l, top}, {r, top}, {r, bottom}, {l, bottom}};

        scan_convex(corners, 4, t->clip, flat, &(Flat){t, colour});

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
