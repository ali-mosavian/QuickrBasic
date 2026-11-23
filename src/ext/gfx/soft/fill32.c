/* 32 bits a pixel's fillers (fill.h): &HAARRGGBB, alpha under a half the key. What is drawn into
   a 32-bit surface is opaque. */
#include "soft/fill.h"

static inline uint32_t load(const uint8_t *const p)
{
    uint32_t v;
    __builtin_memcpy(&v, p, 4);
    return v;
}

static inline void store(uint8_t *const p, const uint32_t v) { __builtin_memcpy(p, &v, 4); }

static void span(uint8_t *const d, const int32_t n, const uint32_t colour)
{
    for (int32_t i = 0; i < n; i++)
        store(d + 4 * i, colour);
}

/* n texels from s, stepping by dir; key skips those under half alpha. */
static inline void row(
    uint8_t *const d,
    const uint8_t *const s,
    const int32_t n,
    const int32_t dir,
    const bool key
)
{
    for (int32_t i = 0; i < n; i++) {
        const uint32_t v = load(s + 4 * i * dir);

        if (!key || !px_key(PX_32, v))
            store(d + 4 * i, v);
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
    row(d, s, n, 1, false);
}

static void opaque_flipped(uint8_t *const d, const uint8_t *const s, const int32_t n)
{
    row(d, s, n, -1, false);
}

static inline uint32_t next(const Pixels *const s, Steps *const st)
{
    const uint32_t v = load(s->p + (st->v >> 16) * (int32_t)s->pitch + 4 * (st->u >> 16));
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
        const uint32_t v = next(s, &st);

        if (!key || !px_key(PX_32, v))
            store(d + 4 * i, v);
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

static void fetch(uint32_t *const out, const Pixels *const s, Steps st, const int32_t n)
{
    for (int32_t i = 0; i < n; i++)
        out[i] = next(s, &st);
}

static void ink(uint8_t *const d, const uint32_t *const texels, const int32_t n, const Ink *const k)
{
    uint32_t argb[FILL_CHUNK];
    bool drawn[FILL_CHUNK];
    fill_argb(argb, drawn, texels, n, k);

    for (int32_t i = 0; i < n; i++)
        if (drawn[i])
            store(
                d + 4 * i,
                k->mode ? fill_blend(argb[i], load(d + 4 * i), k) : argb[i] | PX_OPAQUE
            );
}

static uint32_t paint(const uint32_t texel, const Ink *const k)
{
    uint32_t argb;
    bool drawn;
    fill_argb(&argb, &drawn, &texel, 1, k);
    return argb | PX_OPAQUE;
}

const Fill fill32 = {
    .span = span,
    .row =
        {
            [0] = keyed,
            [ROW_FLIPX] = keyed_flipped,
            [ROW_OPAQUE] = opaque,
            [ROW_OPAQUE | ROW_FLIPX] = opaque_flipped,
        },
    .step = {[0] = stepped_keyed, [STEP_OPAQUE] = stepped_opaque},
    .fetch = fetch,
    .ink = ink,
    .paint = paint,
};
