/* Steps: a walk over a picture's texels, u and v 16.16, a constant step a pixel. Every textured
 * span walks one: the fillers along it unchecked, when it stays inside the picture; a pixel at a
 * time held inside (steps_texel), when it may not.
 */
#ifndef GFX_STEPS_H
#define GFX_STEPS_H
#include "base/pixels.h"

/* The first texel's u and v, and the step a pixel. wrap: u repeats every wrap (16.16), and stays
   in [0, wrap); 0 never. */
typedef struct {
    int32_t u, v, du, dv, wrap;
} Steps;

/* u wrapped into [0, w): a step past an end, by a width, without a divide. */
static inline int32_t steps_wrap(int32_t u, const int32_t w)
{
    u = u >= w ? u - w : u < 0 ? u + w : u;

    if (u >= 0 && u < w)
        return u;

    const int32_t m = u % w;
    return m < 0 ? m + w : m;
}

/* The walk from u, v by du, dv a pixel, u wrapped when it wraps. */
static inline Steps steps_from(
    const int32_t u,
    const int32_t v,
    const int32_t du,
    const int32_t dv,
    const int32_t wrap
)
{
    return (Steps){wrap ? steps_wrap(u, wrap) : u, v, du, dv, wrap};
}

/* The walk a pixel on. */
static inline void steps_on(Steps *const st)
{
    st->u = (int32_t)((uint32_t)st->u + (uint32_t)st->du);
    st->v = (int32_t)((uint32_t)st->v + (uint32_t)st->dv);

    if (st->wrap)
        st->u = steps_wrap(st->u, st->wrap);
}

/* f, 16.16, held a texel inside [0, n) texels. */
static inline bool steps_in(const int64_t f, const int32_t n)
{
    return f >= 0 && f < (int64_t)n << 16;
}

/* Every texel of n pixels' walk inside s, the fillers' walk: u wrapping, or held. */
static inline bool steps_inside(const Steps *const st, const Pixels *const s, const int32_t n)
{
    const int64_t u1 = st->u + (int64_t)st->du * (n - 1);
    const int64_t v1 = st->v + (int64_t)st->dv * (n - 1);

    return steps_in(st->v, s->h) && steps_in(v1, s->h) &&
           (st->wrap || (steps_in(st->u, s->w) && steps_in(u1, s->w)));
}

static inline int32_t steps_held(const int32_t i, const int32_t n)
{
    return i < 0 ? 0 : i >= n ? n - 1 : i;
}

/* The texel at the walk's place, held inside s. */
static inline uint32_t steps_texel(const Pixels *const s, const Steps *const st)
{
    const uint32_t bytes = px_bytes(s->format);
    const int32_t u = steps_held(st->u >> 16, s->w), v = steps_held(st->v >> 16, s->h);

    return px_load(s->p + v * s->pitch + (uint32_t)u * bytes, bytes);
}

/* A walk that does not wrap read texel by texel, inside its picture: its row's address stepped by
   whole rows, v's fraction carried into it, so no texel costs a multiply. The address is a number:
   the last step may leave the picture. */
typedef struct {
    uintptr_t row;
    int32_t down, pitch;
    uint32_t v, dv; /* v's fraction and its step's, in the top 16 bits */
    int32_t u, du;
} StepsRead;

static inline StepsRead steps_read(const Pixels *const s, const Steps st)
{
    const int32_t pitch = (int32_t)s->pitch;

    return (StepsRead){
        .row = (uintptr_t)s->p + (uintptr_t)(intptr_t)((st.v >> 16) * pitch),
        .down = (st.dv >> 16) * pitch,
        .pitch = pitch,
        .v = (uint32_t)st.v << 16,
        .dv = (uint32_t)st.dv << 16,
        .u = st.u,
        .du = st.du,
    };
}

/* The texel at the read's place, bytes a texel, a constant for a format's own load; the read a
   pixel on. */
static inline uint32_t steps_read_next(StepsRead *const r, const uint32_t bytes)
{
    const uint32_t texel = px_load((const uint8_t *)r->row + (r->u >> 16) * (int32_t)bytes, bytes);
    const uint32_t was = r->v;

    r->v += r->dv;
    r->row += (uintptr_t)(intptr_t)(r->down + (-(int32_t)(r->v < was) & r->pitch));
    r->u = (int32_t)((uint32_t)r->u + (uint32_t)r->du);
    return texel;
}

/* The pixels from the walk's place up to where u wraps, at most n: those a walk that does not
   wrap reads alike. */
static inline int32_t steps_unwrapped(const Steps *const st, const int32_t n)
{
    const int32_t m = st->du > 0   ? (st->wrap - 1 - st->u) / st->du + 1
                      : st->du < 0 ? st->u / -st->du + 1
                                   : n;
    return m < n ? m : n;
}
#endif
