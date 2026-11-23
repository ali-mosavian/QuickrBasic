/* A convex polygon to spans (scan.h). */
#include "soft/scan.h"

void scan_convex(
    const ScanPoint *const p,
    const uint32_t n,
    const Rect clip,
    ScanSpan *const span,
    void *const ctx
)
{
    float top = p[0].y, bottom = p[0].y;

    for (uint32_t i = 1; i < n; i++) {
        top = p[i].y < top ? p[i].y : top;
        bottom = p[i].y > bottom ? p[i].y : bottom;
    }

    const int32_t y0 = rect_max(scan_first(top), clip.y);
    const int32_t y1 = rect_min(scan_first(bottom), clip.y + clip.h);

    for (int32_t y = y0; y < y1; y++) {
        float left = 0, right = 0;
        bool crossed = false;

        for (uint32_t i = 0; i < n; i++) {
            const ScanPoint a = p[i], b = p[(i + 1) % n];

            if ((a.y <= (float)y) == (b.y <= (float)y))
                continue;

            const float x = a.x + ((float)y - a.y) * (b.x - a.x) / (b.y - a.y);
            left = !crossed || x < left ? x : left;
            right = !crossed || x > right ? x : right;
            crossed = true;
        }

        const int32_t x0 = rect_max(scan_first(left), clip.x);
        const int32_t x1 = rect_min(scan_first(right), clip.x + clip.w);

        if (crossed && x0 < x1)
            span(ctx, y, x0, x1);
    }
}
