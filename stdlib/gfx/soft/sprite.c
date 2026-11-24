/* Upright sprites (soft.h): a picture's rows copied, or walked through the ink. */
#include "base/cover.h"
#include "gfx_qx.h"
#include "soft/span.h"

/* A draw call's flags as a row filler's variant. */
static uint32_t variant(const int32_t flags)
{
    return (flags & GFX_DRAW_FLIPX ? FILL_FLIPX : 0) | (flags & GFX_PEN_OPAQUE ? FILL_OPAQUE : 0);
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
    const bool plain = span_fused(t, s);
    const Textured walk = {t, s, flags & GFX_PEN_OPAQUE, false, false, false};

    for (int32_t y_ = drawn.y; y_ < drawn.y + drawn.h; y_++) {
        const int32_t v = flip_y ? s->h - 1 - (y_ - top) : y_ - top;
        const Steps st = {u << 16, v << 16, flip_x ? -65536 : 65536, 0, 0};

        if (plain)
            row(span_at(t, drawn.x, y_), s->p + v * s->pitch + u * px_bytes(s->format), drawn.w);
        else
            span_texels(&walk, drawn.x, y_, drawn.w, st, 0, 0);
    }
}
