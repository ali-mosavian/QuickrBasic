/* 15 and 16 bits a pixel's fillers (fill.h): one template, built for each format's masks. A
   pixel 0 is the key. */
#include "core/mem.h"
#include "soft/fill.h"

static inline uint16_t load(const uint8_t *const p)
{
    uint16_t v;
    __builtin_memcpy(&v, p, 2);
    return v;
}

static inline void store(uint8_t *const p, const uint16_t v) { __builtin_memcpy(p, &v, 2); }

static void span(uint8_t *const d, const int32_t n, const uint32_t colour)
{
    for (int32_t i = 0; i < n; i++)
        store(d + 2 * i, (uint16_t)colour);
}

/* n texels from s, stepping by dir; key skips 0. */
static inline void row(
    uint8_t *const d,
    const uint8_t *const s,
    const int32_t n,
    const int32_t dir,
    const bool key
)
{
    for (int32_t i = 0; i < n; i++) {
        const uint16_t v = load(s + 2 * i * dir);

        if (!key || v)
            store(d + 2 * i, v);
    }
}

static void keyed(uint8_t *const d, const uint8_t *const s, const int32_t n)
{
    row(d, s, n, 1, true);
}

static void keyed_flipped(uint8_t *const d, const uint8_t *const s, const int32_t n)
{
    row(d, s, n, -1, true);
}

static void opaque(uint8_t *const d, const uint8_t *const s, const int32_t n)
{
    mem_copy(d, s, 2 * (size_t)n);
}

static void opaque_flipped(uint8_t *const d, const uint8_t *const s, const int32_t n)
{
    row(d, s, n, -1, false);
}

static inline uint16_t next(const Pixels *const s, Steps *const st)
{
    const uint16_t v = load(s->p + (st->v >> 16) * (int32_t)s->pitch + 2 * (st->u >> 16));
    st->u += st->du;
    st->v += st->dv;

    if (st->wrap)
        st->u = fill_wrap(st->u, st->wrap);

    return v;
}

static void stepped(
    uint8_t *const d,
    const Pixels *const s,
    Steps st,
    const int32_t n,
    const bool key
)
{
    for (int32_t i = 0; i < n; i++) {
        const uint16_t v = next(s, &st);

        if (!key || v)
            store(d + 2 * i, v);
    }
}

static void stepped_keyed(uint8_t *const d, const Pixels *const s, const Steps st, const int32_t n)
{
    stepped(d, s, st, n, true);
}

static void stepped_opaque(uint8_t *const d, const Pixels *const s, const Steps st, const int32_t n)
{
    stepped(d, s, st, n, false);
}

/* hLineT's walk, what dos/plxt16.asm must draw: t read here, patched into the assembly. */
static void fetch(uint32_t *const out, const Pixels *const s, Steps st, const int32_t n)
{
    for (int32_t i = 0; i < n; i++)
        out[i] = next(s, &st);
}

static inline void ink(
    const PxFormat f,
    uint8_t *const d,
    const uint32_t *const texels,
    const int32_t n,
    const Ink *const k
)
{
    uint32_t argb[FILL_CHUNK];
    bool drawn[FILL_CHUNK];
    fill_argb(argb, drawn, texels, n, k);

    for (int32_t i = 0; i < n; i++) {
        if (!drawn[i])
            continue;

        const uint32_t c = k->mode ? fill_blend(argb[i], px_argb(f, load(d + 2 * i)), k) : argb[i];
        store(d + 2 * i, (uint16_t)px_pack(f, c));
    }
}

static inline uint32_t paint(const PxFormat f, const uint32_t texel, const Ink *const k)
{
    uint32_t argb;
    bool drawn;
    fill_argb(&argb, &drawn, &texel, 1, k);
    return px_pack(f, argb);
}

static void ink15(
    uint8_t *const d,
    const uint32_t *const texels,
    const int32_t n,
    const Ink *const k
)
{
    ink(PX_15, d, texels, n, k);
}

static void ink16(
    uint8_t *const d,
    const uint32_t *const texels,
    const int32_t n,
    const Ink *const k
)
{
    ink(PX_16, d, texels, n, k);
}

static uint32_t paint15(const uint32_t texel, const Ink *const k) { return paint(PX_15, texel, k); }

static uint32_t paint16(const uint32_t texel, const Ink *const k) { return paint(PX_16, texel, k); }

#define FILL16(inked, painted)                                                                     \
    {                                                                                              \
        .span = span,                                                                              \
        .row =                                                                                     \
            {                                                                                      \
                [0] = keyed,                                                                       \
                [ROW_FLIPX] = keyed_flipped,                                                       \
                [ROW_OPAQUE] = opaque,                                                             \
                [ROW_OPAQUE | ROW_FLIPX] = opaque_flipped,                                         \
            },                                                                                     \
        .step = {[0] = stepped_keyed, [STEP_OPAQUE] = stepped_opaque},                             \
        .fetch = fetch,                                                                            \
        .ink = inked,                                                                              \
        .paint = painted,                                                                          \
    }

const Fill fill15 = FILL16(ink15, paint15);
const Fill fill16 = FILL16(ink16, paint16);
