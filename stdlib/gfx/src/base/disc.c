/* A disc's rows as runs (disc.h). */
#include "base/disc.h"
#include "base/cover.h"

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

/* The run's pixels in the sweep, a run each. */
static void swept_runs(
    const Disc *const k,
    const Sweep *const w,
    const int32_t y,
    const int32_t x0,
    const int32_t x1,
    DiscRun *const run,
    void *const ctx
)
{
    const float dy = (float)y - k->c.y;

    for (int32_t x = x0; x < x1;) {
        while (x < x1 && !swept(w, (float)x - k->c.x, dy))
            x++;

        const int32_t from = x;

        while (x < x1 && swept(w, (float)x - k->c.x, dy))
            x++;

        if (from < x)
            run(ctx, y, from, x);
    }
}

/* Row y's run x0 up to x1, in w unless null. */
static void piece(
    const Disc *const k,
    const Sweep *const w,
    const int32_t y,
    const int32_t x0,
    const int32_t x1,
    DiscRun *const run,
    void *const ctx
)
{
    if (w)
        swept_runs(k, w, y, x0, x1, run, ctx);
    else if (x0 < x1)
        run(ctx, y, x0, x1);
}

void disc_runs(
    const Disc *const k,
    const Sweep *const w,
    const int32_t top,
    const int32_t bottom,
    DiscRun *const run,
    void *const ctx
)
{
    const int32_t first = cover_first(k->c.y - k->outer) - 1,
                  last = cover_first(k->c.y + k->outer) + 1;
    const int32_t y0 = first > top ? first : top, y1 = last < bottom ? last : bottom;

    for (int32_t y = y0; y < y1; y++) {
        int32_t a0, a1, b0, b1;

        if (!chord(k->c, k->outer, y, &a0, &a1))
            continue;

        if (!chord(k->c, k->inner, y, &b0, &b1)) {
            piece(k, w, y, a0, a1, run, ctx);
            continue;
        }

        piece(k, w, y, a0, b0, run, ctx);
        piece(k, w, y, b1, a1, run, ctx);
    }
}
