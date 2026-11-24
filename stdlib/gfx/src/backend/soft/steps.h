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

/* The walk n pixels on. */
static inline Steps steps_skip(Steps st, const int32_t n)
{
    const int64_t u = st.u + (int64_t)st.du * n;
    st.u = st.wrap ? steps_wrap((int32_t)(u % st.wrap), st.wrap) : (int32_t)u;
    st.v = (int32_t)(st.v + (int64_t)st.dv * n);
    return st;
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

/* The texel at the walk's place, inside s, bytes a texel; the walk a pixel on. A constant bytes
   makes it a format's own load. */
static inline uint32_t steps_next(const Pixels *const s, Steps *const st, const uint32_t bytes)
{
    const uint32_t texel =
        px_load(s->p + (st->v >> 16) * (int32_t)s->pitch + (st->u >> 16) * (int32_t)bytes, bytes);

    steps_on(st);
    return texel;
}
#endif
