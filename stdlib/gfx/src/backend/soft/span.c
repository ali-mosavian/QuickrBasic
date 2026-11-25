/* Spans (span.h). */
#include "backend/soft/span.h"

uint8_t *span_at(const SoftTarget *const t, const int32_t x, const int32_t y)
{
    return t->px.p + y * t->px.pitch + x * px_bytes(t->px.format);
}

bool span_fused(const SoftTarget *const t, const Pixels *const s)
{
    return !t->ink.effects && s->format == t->px.format;
}

Flat span_flat_of(const SoftTarget *const t, const uint32_t colour)
{
    Flat f = {.r = {.dst = t->px, .ink = &t->ink}};

    f.fill = t->fill->pick_flat(&f.r, colour, t->tints);
    return f;
}

void span_flat(
    const SoftTarget *const t,
    const Flat *const f,
    const int32_t y,
    const int32_t x0,
    const int32_t x1
)
{
    const int32_t from = rect_max(x0, t->clip.x), to = rect_min(x1, t->clip.x + t->clip.w);

    if (y >= t->clip.y && y < t->clip.y + t->clip.h && from < to)
        f->fill(&f->r, y, from, to, 0);
}
