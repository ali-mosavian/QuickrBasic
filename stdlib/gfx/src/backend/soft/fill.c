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

/* A texel's index: SOLID's, its own through the lut, or a direct one's colour shaded and
   matched. */
[[gnu::always_inline]]
static inline uint32_t indexed(const uint32_t texel, const Ink *const k)
{
    if (k->solid)
        return k->colour;

    if (k->source == PX_8)
        return k->lut ? k->lut[texel] : texel;

    return k->map[px_cell(px_shade(px_argb(k->source, texel), k->shade))];
}

/* ARGB as a pixel of direct format f, opaque as all drawn into a surface is. */
[[gnu::always_inline]]
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

/* Where a filler's pixels come from: its walk through the ink (INKED) or not (STEPPED), a row's
   texels in order (ROW), one pixel (SPAN), or a colour blended over the pixel there (TINT). */
typedef enum { PUT_INKED, PUT_STEPPED, PUT_ROW, PUT_SPAN, PUT_TINT } Put;

/* A filler's: the walk, the ink, a row's next texel and the bytes to the one after, SPAN's pixel,
   TINT's colour (an index, or ARGB) and at 8 bits its blend row. Copied in: through a pointer each
   store could change it, for all the compiler knows. */
typedef struct {
    StepsRead r;
    Ink ink;
    const uint8_t *s;
    int32_t dir;
    uint32_t colour;
    const uint8_t *row;
} Source;

/* put's next pixel of f, from g as variant v, over there, f's pixel when it reads one; *texel the
   texel it was made of: false, keyed. */
[[gnu::always_inline]]
static inline bool put_next(
    const Put put,
    const PxFormat f,
    const PxFormat g,
    const uint32_t v,
    Source *const s,
    const uint32_t there,
    uint32_t *const out,
    uint32_t *const texel
)
{
    const Ink *const k = &s->ink;
    const bool add = k->mode == BLEND_ADD;

    if (put == PUT_SPAN) {
        *out = s->colour;
        return true;
    }

    if (put == PUT_TINT) {
        const uint32_t d = px_argb(f, there);
        *out = f == PX_8 ? s->row[there]
               : add     ? direct(f, px_mix(s->colour, d, BLEND_ADD, k->alpha))
                         : direct(f, px_mix(s->colour, d, BLEND_MIX, k->alpha));
        return true;
    }

    const PxFormat from = put == PUT_INKED ? g : f;
    uint32_t t;

    if (put == PUT_ROW) {
        t = px_load(s->s, px_bytes(f));
        s->s += s->dir;
    } else
        t = steps_read_next(&s->r, px_bytes(from));

    *texel = t;

    if (v & FILL_KEYED && px_key(from, t))
        return false;

    if (put != PUT_INKED) {
        *out = t;
        return true;
    }

    if (f == PX_8) {
        const uint32_t c = v & FILL_SOLID ? k->colour
                           : g != PX_8    ? k->map[px_cell(px_shade(px_argb(g, t), k->shade))]
                           : v & FILL_LUT ? k->lut[t]
                                          : t;
        *out = v & FILL_BLENDED ? k->blend[c << 8 | there] : c;
        return true;
    }

    const uint32_t c = v & FILL_SOLID ? k->colour
                       : g == PX_8    ? k->argb[t & 0xFF]
                                      : px_shade(px_argb(g, t), k->shade);
    *out = !(v & FILL_BLENDED) ? direct(f, c)
           : add               ? direct(f, px_mix(c, px_argb(f, there), BLEND_ADD, k->alpha))
                               : direct(f, px_mix(c, px_argb(f, there), BLEND_MIX, k->alpha));
    return true;
}

/* A row's next word of f's pixels, read at once: backward when dir is, its pixels then reversed. */
[[gnu::always_inline]]
static inline uint32_t row_word(const PxFormat f, Source *const s)
{
    const uint32_t b = px_bytes(f);
    const bool back = s->dir < 0;
    const uint32_t w = px_load(s->s - (back ? 4 - (int32_t)b : 0), 4);

    s->s += s->dir * (int32_t)(4 / b);

    if (!back)
        return w;

    return b == 1 ? __builtin_bswap32(w) : w >> 16 | w << 16;
}

[[gnu::always_inline]]
static inline void put_one(
    const Put put,
    const PxFormat f,
    const PxFormat g,
    const uint32_t v,
    Source *const s,
    uint8_t *const p,
    const bool reads
)
{
    const uint32_t b = px_bytes(f);
    uint32_t c, t;

    if (put_next(put, f, g, v, s, reads ? px_load(p, b) : 0, &c, &t))
        px_store(p, c, b);
}

[[gnu::always_inline]]
static inline uint32_t rotated(const uint32_t w, const uint32_t bits)
{
    return w >> bits | w << (32 - bits);
}

/* The top bits of w's lanes of bits, 8 or 16, that are 0; the rest 0. */
[[gnu::always_inline]]
static inline uint32_t zero_tops(const uint32_t w, const uint32_t bits)
{
    const uint32_t low = bits == 8 ? 0x7F7F7F7Fu : 0x7FFF7FFFu;

    return ~((w & low) + low | w | low);
}

/* n pixels of put into d, f's, from g as variant v: once d is aligned, a word of them at a time,
   read at once when keyed or blended, built a pixel at a time into its top (uGL's or and ror), and
   stored at once, the keyed kept as they were. A texel as wide as the pixel is keyed when 0, so a
   word of them is keyed at once, its lanes that are 0, and stored as it is when none is. */
[[gnu::always_inline]]
static inline void words_of(
    const Put put,
    const PxFormat f,
    const PxFormat g,
    const uint32_t v,
    uint8_t *const d,
    const int32_t n,
    Source s
)
{
    const uint32_t b = px_bytes(f), per = 4 / b, bits = 8 * b,
                   mask = b == 4 ? ~0u : (1u << bits) - 1;
    const bool blends = put == PUT_TINT || v & FILL_BLENDED;
    const bool lanes = v & FILL_KEYED && px_bytes(g) == b;
    const bool reads =
        blends || (v & FILL_KEYED && !lanes); /* a word's pixels, before it is made */
    const uint32_t each = lanes ? v & ~(uint32_t)FILL_KEYED : v;
    int32_t i = 0;

    for (; per > 1 && i < n && (uintptr_t)(d + i * (int32_t)b) & 3; i++)
        put_one(put, f, g, v, &s, d + i * (int32_t)b, blends);

    const int32_t words = per > 1 ? (n - i) / (int32_t)per : 0;
    uint8_t *const end = d + (i + words * (int32_t)per) * (int32_t)b;

    for (int32_t k = -words; k; k++) {
        uint8_t *const p = end + 4 * k;
        const uint32_t old = reads ? px_load(p, 4) : 0;
        uint32_t word = 0, keep = 0, texels = 0, rest = old;

        if (put == PUT_ROW)
            word = row_word(f, &s);
        else
            for (uint32_t j = 0; j < per; j++) {
                uint32_t c, t;
                const bool drawn = put_next(put, f, g, each, &s, rest & mask, &c, &t);

                rest >>= bits & 31;
                word = rotated(word | (drawn ? c : 0), bits);
                texels = put == PUT_INKED && lanes ? rotated(texels | t, bits) : 0;
                keep = lanes ? 0 : rotated(keep | (drawn ? 0 : mask), bits);
            }

        if (lanes) {
            const uint32_t top = zero_tops(put == PUT_INKED ? texels : word, bits);

            if (!top) {
                px_store(p, word, 4);
                continue;
            }

            keep = (top << 1) - (top >> (bits - 1)); /* the top bits widened down their lanes */
        }

        px_store(p, (word & ~keep) | ((reads ? old : px_load(p, 4)) & keep), 4);
    }

    for (i += words * (int32_t)per; i < n; i++)
        put_one(put, f, g, v, &s, d + i * (int32_t)b, blends);
}

[[gnu::always_inline]]
static inline void span_of(const PxFormat f, uint8_t *const d, const int32_t n, const uint32_t c)
{
    if (f == PX_8)
        mem_fill(d, (uint8_t)c, (size_t)n);
    else
        words_of(PUT_SPAN, f, f, 0, d, n, (Source){.colour = c});
}

/* n texels from s, each the next dir texels on; key skips px_key's. */
[[gnu::always_inline]]
static inline void row_of(
    const PxFormat f,
    uint8_t *const d,
    const uint8_t *const s,
    const int32_t n,
    const int32_t dir,
    const bool key
)
{
    const int32_t b = (int32_t)px_bytes(f);

    if (!key && dir == 1)
        mem_copy(d, s, (size_t)(b * n));
    else
        words_of(PUT_ROW, f, f, key ? FILL_KEYED : 0, d, n, (Source){.s = s, .dir = dir * b});
}

[[gnu::always_inline]]
static inline void steps_of(
    const PxFormat f,
    uint8_t *const d,
    const Pixels *const s,
    const Steps st,
    const int32_t n,
    const bool key
)
{
    const StepsBy by = steps_by(s, st);

    words_of(PUT_STEPPED, f, f, key ? FILL_KEYED : 0, d, n, (Source){.r = steps_read(s, st, &by)});
}

/* ink_of's texels fetched as drawn, from g into f as variant v. */
[[gnu::always_inline]]
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
    const StepsBy by = steps_by(s, st);

    words_of(PUT_INKED, f, g, v, d, n, (Source){.r = steps_read(s, st, &by), .ink = *k});
}

/* ink_of's n texels all colour, blended: its colour made once. */
[[gnu::always_inline]]
static inline void tint_of(
    const PxFormat f,
    uint8_t *const d,
    const int32_t n,
    const uint32_t colour,
    const Ink *const k
)
{
    Source s = {.ink = *k};

    if (f == PX_8)
        s.row = k->blend + (indexed(colour, k) << 8);
    else {
        bool drawn;
        fill_argb(&s.colour, &drawn, &colour, 1, k);
    }

    words_of(PUT_TINT, f, f, FILL_BLENDED, d, n, s);
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
