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
    const Steps st,
    const int32_t n,
    const bool key
)
{
    const uint32_t b = px_bytes(f);
    StepsRead r = steps_read(s, st);

    for (int32_t i = 0; i < n; i++) {
        const uint32_t v = steps_read_next(&r, b);

        if (!key || !px_key(f, v))
            px_store(d + i * (int32_t)b, v, b);
    }
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

/* ink_of's texels fetched as drawn, from g into f as variant v. The ink is copied: through its
   pointer each store into d could change it, for all the compiler knows. */
static inline void inked_of(
    const PxFormat f,
    const PxFormat g,
    const uint32_t v,
    uint8_t *const d,
    const Pixels *const s,
    const Steps st,
    const int32_t n,
    const Ink *const k
)
{
    const Ink ink = *k;
    const uint32_t b = px_bytes(f);
    const bool add = ink.mode == BLEND_ADD;
    StepsRead r = steps_read(s, st);

    for (int32_t i = 0; i < n; i++) {
        const uint32_t t = steps_read_next(&r, px_bytes(g));
        uint8_t *const p = d + i * (int32_t)b;

        if (v & FILL_KEYED && px_key(g, t))
            continue;

        if (f == PX_8) {
            const uint32_t c = v & FILL_SOLID ? ink.colour
                               : g != PX_8    ? ink.map[px_cell(px_shade(px_argb(g, t), ink.shade))]
                               : v & FILL_LUT ? ink.lut[t]
                                              : t;
            *p = v & FILL_BLENDED ? ink.blend[c << 8 | *p] : (uint8_t)c;
            continue;
        }

        const uint32_t c = v & FILL_SOLID ? ink.colour
                           : g == PX_8    ? ink.argb[t & 0xFF]
                                          : px_shade(px_argb(g, t), ink.shade);
        const uint32_t there = v & FILL_BLENDED ? px_argb(f, px_load(p, b)) : 0;
        const uint32_t out = !(v & FILL_BLENDED) ? c
                             : add               ? px_mix(c, there, BLEND_ADD, ink.alpha)
                                                 : px_mix(c, there, BLEND_MIX, ink.alpha);
        px_store(p, direct(f, out), b);
    }
}

/* ink_of's n texels all colour, blended: its colour made once. */
static inline void tint_of(
    const PxFormat f,
    uint8_t *const d,
    const int32_t n,
    const uint32_t colour,
    const Ink *const k
)
{
    if (f == PX_8) {
        const uint8_t *const row = k->blend + (indexed(colour, k) << 8);

        for (int32_t i = 0; i < n; i++)
            d[i] = row[d[i]];

        return;
    }

    const uint32_t b = px_bytes(f);
    const uint8_t alpha = k->alpha;
    uint32_t c;
    bool drawn;
    fill_argb(&c, &drawn, &colour, 1, k);

    if (k->mode == BLEND_ADD)
        for (int32_t i = 0; i < n; i++) {
            uint8_t *const p = d + i * (int32_t)b;
            px_store(p, direct(f, px_mix(c, px_argb(f, px_load(p, b)), BLEND_ADD, alpha)), b);
        }
    else
        for (int32_t i = 0; i < n; i++) {
            uint8_t *const p = d + i * (int32_t)b;
            px_store(p, direct(f, px_mix(c, px_argb(f, px_load(p, b)), BLEND_MIX, alpha)), b);
        }
}

/* Variant k s l b: keyed, SOLID, LUT, BLENDED, each 0 or 1. */
#define FILL_VARIANT(k, s, l, b)                                                                   \
    ((k) * FILL_KEYED | (s) * FILL_SOLID | (l) * FILL_LUT | (b) * FILL_BLENDED)

/* name's inked filler of variant k s l b, from g into f; its slot in the table's row. */
#define FILL_INKED(name, f, g, k, s, l, b)                                                         \
    static void name##_##k##s##l##b(                                                               \
        uint8_t *const d,                                                                          \
        const Pixels *const p,                                                                     \
        const Steps st,                                                                            \
        const int32_t n,                                                                           \
        const Ink *const ink                                                                       \
    )                                                                                              \
    {                                                                                              \
        inked_of(f, g, FILL_VARIANT(k, s, l, b), d, p, st, n, ink);                                \
    }
#define FILL_SLOT(name, f, g, k, s, l, b) [FILL_VARIANT(k, s, l, b)] = name##_##k##s##l##b,

/* X for a variant keyed and not. */
#define FILL_KEYS(X, s, l, b, ...) X(__VA_ARGS__, 0, s, l, b) X(__VA_ARGS__, 1, s, l, b)
/* X for those SOLID and not. */
#define FILL_SOLIDS(X, l, b, ...)                                                                  \
    FILL_KEYS(X, 0, l, b, __VA_ARGS__) FILL_KEYS(X, 1, l, b, __VA_ARGS__)
/* Most pairs': keyed, SOLID and BLENDED, each or not. */
#define FILL_PLAIN(X, ...) FILL_SOLIDS(X, 0, 0, __VA_ARGS__) FILL_SOLIDS(X, 0, 1, __VA_ARGS__)
/* 8 bits from 8 bits: those, and LUT's, which SOLID's colour overrides. */
#define FILL_EIGHT(X, ...)                                                                         \
    FILL_PLAIN(X, __VA_ARGS__) FILL_KEYS(X, 0, 1, 0, __VA_ARGS__) FILL_KEYS(X, 0, 1, 1, __VA_ARGS__)

/* Every target, by X: its table's name and format, and its variants for 8-bit texels. */
#define FILL_TARGETS(X, ...)                                                                       \
    X(fill8, PX_8, FILL_EIGHT, __VA_ARGS__)                                                        \
    X(fill15, PX_15, FILL_PLAIN, __VA_ARGS__)                                                      \
    X(fill16, PX_16, FILL_PLAIN, __VA_ARGS__)                                                      \
    X(fill32, PX_32, FILL_PLAIN, __VA_ARGS__)
/* Target t's every source, by Y: the fillers' name, the formats, the variants. */
#define FILL_SOURCES(t, f, eight, Y)                                                               \
    Y(t##_from8, f, PX_8, eight)                                                                   \
    Y(t##_from15, f, PX_15, FILL_PLAIN)                                                            \
    Y(t##_from16, f, PX_16, FILL_PLAIN)                                                            \
    Y(t##_from32, f, PX_32, FILL_PLAIN)
#define FILL_PAIR(name, f, g, variants) variants(FILL_INKED, name, f, g)
#define FILL_ROW(name, f, g, variants) [f][g] = {variants(FILL_SLOT, name, f, g)},

FILL_TARGETS(FILL_SOURCES, FILL_PAIR)

/* By target, source and variant; a variant fill_variant never gives, null. */
static FillInked *const fill_inked[PX_FORMATS][PX_FORMATS][FILL_INKS] = {
    FILL_TARGETS(FILL_SOURCES, FILL_ROW)
};
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
    static void                                                                                    \
    name##_tint(uint8_t *const d, const int32_t n, const uint32_t colour, const Ink *const k)      \
    {                                                                                              \
        tint_of(f, d, n, colour, k);                                                               \
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
        .ink = name##_ink,                                                                         \
        .paint = name##_paint,                                                                     \
        .inked = fill_inked[f],                                                                    \
        .tint = name##_tint,                                                                       \
    }

#define FILL_FORMAT(t, f, ...) FILL_TABLE(t, f);
#define FILL_ENTRY(t, f, ...) [f] = &t,

FILL_TARGETS(FILL_FORMAT)

const Fill *const fill_formats[PX_FORMATS] = {FILL_TARGETS(FILL_ENTRY)};
