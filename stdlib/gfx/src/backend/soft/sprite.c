/* Upright sprites (soft.h): a surface's rows copied, or walked through the ink. */
#include "base/cover.h"
#include "gfx_qx.h"
#include "backend/soft/span.h"

/* A draw call's flags as a row filler's variant. */
static uint32_t variant(const int32_t flags)
{
    return (flags & GFX_DRAW_FLIP_X ? FILL_FLIPX : 0) | (flags & GFX_DRAW_OPAQUE ? FILL_OPAQUE : 0);
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
    const bool flip_x = flags & GFX_DRAW_FLIP_X, flip_y = flags & GFX_DRAW_FLIP_Y;
    const int32_t u = flip_x ? s->w - 1 - (drawn.x - left) : drawn.x - left;
    const bool plain = span_fused(t, s);
    RowAffine *const inked =
        t->fill->inked[s->format][fill_variant(&t->ink, t->px.format, s->format)];
    const Rows r = {
        .dst = t->px,
        .src = *s,
        .ink = &t->ink,
        .du = flip_x ? -65536 : 65536,
    };

    for (int32_t y_ = drawn.y; y_ < drawn.y + drawn.h; y_++) {
        const int32_t v = flip_y ? s->h - 1 - (y_ - top) : y_ - top;

        if (plain)
            row(span_at(t, drawn.x, y_), s->p + v * s->pitch + u * px_bytes(s->format), drawn.w);
        else
            inked(&r, y_, drawn.x, drawn.x + drawn.w, u << 16, v << 16, 0);
    }
}
