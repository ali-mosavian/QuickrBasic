/* Steps: a walk over a texture's texels, u and v 16.16, a constant step a pixel. Every textured
 * span walks one: the fillers along it unchecked, when it stays inside the texture; a pixel at a
 * time held inside (steps_texel), when it may not.
 */
#ifndef GFX_STEPS_H
#define GFX_STEPS_H
#include "base/pixels.h"

/* The first texel's u and v, and the step a pixel. wrap: u repeats every wrap (16.16), and stays
   in [0, wrap); wrap_v, v the same; 0 never. */
typedef struct {
    int32_t u, v, du, dv, wrap, wrap_v;
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

/* The walk from u, v by du, dv a pixel, each wrapped when it wraps. */
static inline Steps steps_from(
    const int32_t u,
    const int32_t v,
    const int32_t du,
    const int32_t dv,
    const int32_t wrap,
    const int32_t wrap_v
)
{
    return (Steps){
        wrap ? steps_wrap(u, wrap) : u,
        wrap_v ? steps_wrap(v, wrap_v) : v,
        du,
        dv,
        wrap,
        wrap_v,
    };
}

/* The walk a pixel on. */
static inline void steps_on(Steps *const st)
{
    st->u = (int32_t)((uint32_t)st->u + (uint32_t)st->du);
    st->v = (int32_t)((uint32_t)st->v + (uint32_t)st->dv);

    if (st->wrap)
        st->u = steps_wrap(st->u, st->wrap);

    if (st->wrap_v)
        st->v = steps_wrap(st->v, st->wrap_v);
}

/* f, 16.16, held a texel inside [0, n) texels. */
static inline bool steps_in(const int64_t f, const int32_t n)
{
    return f >= 0 && f < (int64_t)n << 16;
}

/* steps_in's for n under 32768, f 32 bits: n's 16.16 fits an unsigned compare, f's sign with it. */
static inline bool steps_in32(const int32_t f, const int32_t n)
{
    return (uint32_t)f < (uint32_t)n << 16;
}

/* Every texel of n pixels' walk inside s: the plain fillers' walk, not the held ones'. In 32 bits
   unless a side or an end needs more. */
static inline bool steps_inside(const Steps *const st, const Pixels *const s, const int32_t n)
{
    int32_t du, dv, u1, v1;

    if (s->w > 32767 || s->h > 32767 || __builtin_mul_overflow(st->du, n - 1, &du) ||
        __builtin_mul_overflow(st->dv, n - 1, &dv) || __builtin_add_overflow(st->u, du, &u1) ||
        __builtin_add_overflow(st->v, dv, &v1))
        return steps_in(st->v, s->h) && steps_in(st->v + (int64_t)st->dv * (n - 1), s->h) &&
               steps_in(st->u, s->w) && steps_in(st->u + (int64_t)st->du * (n - 1), s->w);

    return steps_in32(st->v, s->h) && steps_in32(v1, s->h) && steps_in32(st->u, s->w) &&
           steps_in32(u1, s->w);
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

#if defined(__i386__) || defined(__x86_64__)
/* x86's: a walk that does not wrap read texel by texel, inside its texture: the texel's address
   stepped by the walk's whole texels and rows, then a row more when v's fraction carries and a
   texel more when u's does (Abrash's). No texel costs a multiply. The fractions share a register,
   uGL's halves: u's the low one, stepped by a 16-bit add, v's pre-shifted to the top, by a 32-bit
   one. The address counts texels, so u's carry is one at every depth: a texture's texels and rows
   are whole texels from 0. The address is a number: the last step may leave the texture. */

typedef struct {
    intptr_t step, pitch; /* in texels */
    uint32_t dv;          /* its low half 0 */
    uint16_t du;
} StepsBy;

/* The steps apart, uGL's immediates: the asm reads them where they are, and would keep the place
   and fractions there too, stored a pixel, if they were beside them. */
typedef struct {
    uintptr_t at;
    uint32_t uv;
    const StepsBy *by;
} StepsRead;

/* The walk's steps over s, bytes a texel: a constant, the filler's format's. */
[[gnu::always_inline]]
static inline StepsBy steps_by(const Pixels *const s, const Steps st, const uint32_t bytes)
{
    const int32_t pitch = (int32_t)(s->pitch / bytes);

    return (StepsBy){
        .step = (st.du >> 16) + (st.dv >> 16) * pitch,
        .pitch = pitch,
        .dv = (uint32_t)st.dv << 16,
        .du = (uint16_t)st.du,
    };
}

[[gnu::always_inline]]
static inline StepsRead steps_read(
    const Pixels *const s,
    const Steps st,
    const StepsBy *const by,
    const uint32_t bytes
)
{
    const int32_t pitch = (int32_t)by->pitch;

    return (StepsRead){
        .at = (uintptr_t)s->p / bytes + (uintptr_t)(intptr_t)((st.v >> 16) * pitch + (st.u >> 16)),
        .uv = (uint32_t)st.v << 16 | (uint16_t)st.u,
        .by = by,
    };
}

/* The texel at the read's place, bytes a texel, a constant for a format's own load; the read a
   pixel on: v's add, its carry a row; u's 16-bit add, its carry a texel. In asm, as uGL's, since a
   compiler will not add into a register's half in place. */
[[gnu::always_inline]]
static inline uint32_t steps_read_next(StepsRead *const r, const uint32_t bytes)
{
    const uint32_t texel = px_load((const uint8_t *)(r->at * bytes), bytes);
    intptr_t row;

    __asm__(
        "addl %[dv], %[uv]\n\t"
        "sbb %[row], %[row]\n\t"
        "and %[pitch], %[row]\n\t"
        "add %[step], %[row]\n\t"
        "addw %[du], %w[uv]\n\t"
        "adc %[row], %[at]"
        : [uv] "+r"(r->uv), [at] "+r"(r->at), [row] "=&r"(row)
        :
        [dv] "m"(r->by->dv), [du] "m"(r->by->du), [pitch] "m"(r->by->pitch), [step] "m"(r->by->step)
        : "cc"
    );
    return texel;
}
#else
/* Elsewhere, 64 bits a register: u and v 16.16 in one, v's pre-shifted to the top half, so one add
   steps both; a texel's place their whole parts by the pitch. On arm64 in asm: ubfx, lsr, madd,
   add. */
typedef struct {
    uint64_t duv;
    uint64_t pitch; /* in texels */
} StepsBy;

/* The steps beside the place, all in registers. */
typedef struct {
    const uint8_t *p;
    uint64_t uv;
    StepsBy by;
} StepsRead;

/* The walk's steps over s, bytes a texel: a constant, the filler's format's. */
[[gnu::always_inline]]
static inline StepsBy steps_by(const Pixels *const s, const Steps st, const uint32_t bytes)
{
    return (StepsBy){
        .duv = ((uint64_t)(uint32_t)st.dv << 32) + (uint64_t)(int64_t)st.du,
        .pitch = s->pitch / bytes,
    };
}

[[gnu::always_inline]]
static inline StepsRead steps_read(
    const Pixels *const s,
    const Steps st,
    const StepsBy *const by,
    [[maybe_unused]] const uint32_t bytes
)
{
    return (StepsRead){
        .p = s->p,
        .uv = (uint64_t)(uint32_t)st.v << 32 | (uint32_t)st.u,
        .by = *by,
    };
}

/* The texel at the read's place, bytes a texel, a constant for a format's own load; the read a
   pixel on. */
[[gnu::always_inline]]
static inline uint32_t steps_read_next(StepsRead *const r, const uint32_t bytes)
{
    uint64_t at;
#if defined(__aarch64__)
    uint64_t u;

    __asm__("ubfx %[u], %[uv], #16, #16\n\t"
            "lsr %[at], %[uv], #48\n\t"
            "madd %[at], %[at], %[pitch], %[u]\n\t"
            "add %[uv], %[uv], %[duv]"
            : [uv] "+r"(r->uv), [at] "=&r"(at), [u] "=&r"(u)
            : [pitch] "r"(r->by.pitch), [duv] "r"(r->by.duv));
#else
    at = (r->uv >> 48) * r->by.pitch + (r->uv >> 16 & 0xFFFF);
    r->uv += r->by.duv;
#endif
    return px_load(r->p + at * bytes, bytes);
}
#endif
#endif
