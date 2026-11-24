/* The fillers (fill.h): one body each, written once for every format and compiled for each, the
   format a constant there. FILL_TABLE binds them into a format's table. */
#include "core/mem.h"
#include "backend/soft/fill.h"

void fill_argb(
    uint32_t *const argb,
    bool *const drawn,
    const uint32_t *const texels,
    const int32_t n,
    const Ink *const k
)
{
    for (int32_t i = 0; i < n; i++) {
        const uint32_t t = texels[i];
        drawn[i] = !k->keyed || !px_key(k->source, t);

        if (k->solid)
            argb[i] = k->colour;
        else if (k->source == PX_8)
            argb[i] = k->argb[t & 0xFF];
        else
            argb[i] = px_shade(px_argb(k->source, t), k->shade);
    }
}

uint32_t fill_blend(const uint32_t s, const uint32_t d, const Ink *const k)
{
    return px_mix(s, d, k->mode, k->alpha);
}

static inline void span_of(const PxFormat f, uint8_t *const d, const int32_t n, const uint32_t c)
{
    const uint32_t b = px_bytes(f);

    if (b == 1) {
        mem_fill(d, (uint8_t)c, (size_t)n);
        return;
    }

    for (int32_t i = 0; i < n; i++)
        px_store(d + i * (int32_t)b, c, b);
}

/* n texels from s, each the next dir texels on; key skips px_key's. */
static inline void row_of(
    const PxFormat f,
    uint8_t *const d,
    const uint8_t *const s,
    const int32_t n,
    const int32_t dir,
    const bool key
)
{
    const uint32_t b = px_bytes(f);

    if (!key && dir == 1) {
        mem_copy(d, s, b * (size_t)n);
        return;
    }

    for (int32_t i = 0; i < n; i++) {
        const uint32_t v = px_load(s + i * dir * (int32_t)b, b);

        if (!key || !px_key(f, v))
            px_store(d + i * (int32_t)b, v, b);
    }
}

static inline void steps_of(
    const PxFormat f,
    uint8_t *const d,
    const Pixels *const s,
    Steps st,
    const int32_t n,
    const bool key
)
{
    const uint32_t b = px_bytes(f);

    for (int32_t i = 0; i < n; i++) {
        const uint32_t v = steps_next(s, &st, b);

        if (!key || !px_key(f, v))
            px_store(d + i * (int32_t)b, v, b);
    }
}

static inline void fetch_of(
    const PxFormat f,
    uint32_t *const out,
    const Pixels *const s,
    Steps st,
    const int32_t n
)
{
    for (int32_t i = 0; i < n; i++)
        out[i] = steps_next(s, &st, px_bytes(f));
}

/* A texel's index: SOLID's, its own through the lut, or a direct one's colour shaded and
   matched. */
static inline uint32_t indexed(const uint32_t texel, const Ink *const k)
{
    if (k->solid)
        return k->colour;

    if (k->source == PX_8)
        return k->lut ? k->lut[texel] : texel;

    return k->map[px_cell(px_shade(px_argb(k->source, texel), k->shade))];
}

/* ARGB as a pixel of direct format f, opaque as all drawn into a surface is. */
static inline uint32_t direct(const PxFormat f, const uint32_t c)
{
    return px_pack(f, c | PX_OPAQUE);
}

static inline uint32_t paint_of(const PxFormat f, const uint32_t texel, const Ink *const k)
{
    if (f == PX_8)
        return indexed(texel, k);

    uint32_t argb;
    bool drawn;
    fill_argb(&argb, &drawn, &texel, 1, k);
    return direct(f, argb);
}

static inline void ink_indexed(
    uint8_t *const d,
    const uint32_t *const texels,
    const int32_t n,
    const Ink *const k
)
{
    for (int32_t i = 0; i < n; i++) {
        if (k->keyed && px_key(k->source, texels[i]))
            continue;

        const uint32_t c = indexed(texels[i], k);
        d[i] = k->blend ? k->blend[c << 8 | d[i]] : (uint8_t)c;
    }
}

static inline void ink_direct(
    const PxFormat f,
    uint8_t *const d,
    const uint32_t *const texels,
    const int32_t n,
    const Ink *const k
)
{
    const uint32_t b = px_bytes(f);
    uint32_t argb[FILL_CHUNK];
    bool drawn[FILL_CHUNK];
    fill_argb(argb, drawn, texels, n, k);

    for (int32_t i = 0; i < n; i++) {
        uint8_t *const p = d + i * (int32_t)b;

        if (drawn[i])
            px_store(
                p,
                direct(f, k->mode ? fill_blend(argb[i], px_argb(f, px_load(p, b)), k) : argb[i]),
                b
            );
    }
}

static inline void ink_of(
    const PxFormat f,
    uint8_t *const d,
    const uint32_t *const texels,
    const int32_t n,
    const Ink *const k
)
{
    if (f == PX_8)
        ink_indexed(d, texels, n, k);
    else
        ink_direct(f, d, texels, n, k);
}

/* Format f's table, name: each body above bound to f. */
#define FILL_TABLE(name, f)                                                                        \
    static void name##_span(uint8_t *const d, const int32_t n, const uint32_t c)                   \
    {                                                                                              \
        span_of(f, d, n, c);                                                                       \
    }                                                                                              \
    static void name##_keyed(uint8_t *const d, const uint8_t *const s, const int32_t n)            \
    {                                                                                              \
        row_of(f, d, s, n, 1, true);                                                               \
    }                                                                                              \
    static void name##_keyed_flipped(uint8_t *const d, const uint8_t *const s, const int32_t n)    \
    {                                                                                              \
        row_of(f, d, s, n, -1, true);                                                              \
    }                                                                                              \
    static void name##_opaque(uint8_t *const d, const uint8_t *const s, const int32_t n)           \
    {                                                                                              \
        row_of(f, d, s, n, 1, false);                                                              \
    }                                                                                              \
    static void name##_opaque_flipped(uint8_t *const d, const uint8_t *const s, const int32_t n)   \
    {                                                                                              \
        row_of(f, d, s, n, -1, false);                                                             \
    }                                                                                              \
    static void                                                                                    \
    name##_stepped_keyed(uint8_t *const d, const Pixels *const s, const Steps st, const int32_t n) \
    {                                                                                              \
        steps_of(f, d, s, st, n, true);                                                            \
    }                                                                                              \
    static void name##_stepped_opaque(                                                             \
        uint8_t *const d,                                                                          \
        const Pixels *const s,                                                                     \
        const Steps st,                                                                            \
        const int32_t n                                                                            \
    )                                                                                              \
    {                                                                                              \
        steps_of(f, d, s, st, n, false);                                                           \
    }                                                                                              \
    static void                                                                                    \
    name##_fetch(uint32_t *const out, const Pixels *const s, const Steps st, const int32_t n)      \
    {                                                                                              \
        fetch_of(f, out, s, st, n);                                                                \
    }                                                                                              \
    static void name##_ink(                                                                        \
        uint8_t *const d,                                                                          \
        const uint32_t *const texels,                                                              \
        const int32_t n,                                                                           \
        const Ink *const k                                                                         \
    )                                                                                              \
    {                                                                                              \
        ink_of(f, d, texels, n, k);                                                                \
    }                                                                                              \
    static uint32_t name##_paint(const uint32_t texel, const Ink *const k)                         \
    {                                                                                              \
        return paint_of(f, texel, k);                                                              \
    }                                                                                              \
    const Fill name = {                                                                            \
        .span = name##_span,                                                                       \
        .row =                                                                                     \
            {                                                                                      \
                [0] = name##_keyed,                                                                \
                [FILL_FLIPX] = name##_keyed_flipped,                                               \
                [FILL_OPAQUE] = name##_opaque,                                                     \
                [FILL_OPAQUE | FILL_FLIPX] = name##_opaque_flipped,                                \
            },                                                                                     \
        .step = {[0] = name##_stepped_keyed, [FILL_OPAQUE] = name##_stepped_opaque},               \
        .fetch = name##_fetch,                                                                     \
        .ink = name##_ink,                                                                         \
        .paint = name##_paint,                                                                     \
    }

FILL_TABLE(fill8, PX_8);
FILL_TABLE(fill15, PX_15);
FILL_TABLE(fill16, PX_16);
FILL_TABLE(fill32, PX_32);

const Fill *const fill_formats[PX_FORMATS] = {
    [PX_8] = &fill8,
    [PX_15] = &fill15,
    [PX_16] = &fill16,
    [PX_32] = &fill32,
};
