/* The inked and tint fillers against ink, the texel-at-a-time path: random texels, tables and
   pixels there, every variant of every target and source. */
#include <string.h>

#include "backend/soft/fill.h"
#include "unit.h"

enum { N = 64 };

static uint32_t next(uint32_t *const seed)
{
    *seed = *seed * 1103515245u + 12345u;
    return *seed >> 8;
}

static void noise(uint32_t *const seed, uint8_t *const p, const uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        p[i] = (uint8_t)next(seed);
}

/* An 8-bit target's lut, map and blend, a direct one's argb, all random. */
typedef struct {
    uint8_t lut[256], map[32768], blend[65536];
    uint32_t argb[256];
} Tables;

/* The ink of variant v from g into f over the tables. */
static Ink ink_of(
    uint32_t *const seed,
    const Tables *const t,
    const PxFormat f,
    const PxFormat g,
    const uint32_t v
)
{
    const bool eight = f == PX_8, blended = v & FILL_BLENDED;

    return (Ink){
        .source = g,
        .keyed = v & FILL_KEYED,
        .solid = v & FILL_SOLID,
        .colour = eight ? next(seed) & 0xFF : PX_OPAQUE | (next(seed) & PX_WHITE),
        .lut = v & FILL_LUT ? t->lut : nullptr,
        .map = t->map,
        .blend = eight && blended ? t->blend : nullptr,
        .argb = t->argb,
        .shade = next(seed) & PX_WHITE,
        .mode = eight || !blended ? BLEND_NONE
                : next(seed) & 1  ? BLEND_ADD
                                  : BLEND_MIX,
        .alpha = (uint8_t)(next(seed) % (PX_FULL + 1)),
    };
}

/* The texels of n pixels' walk from st over s, by its definition: u and v stepped, whole texels. */
static void walked(uint32_t *const out, const Pixels *const s, const Steps st, const int32_t n)
{
    const uint32_t b = px_bytes(s->format);

    for (int32_t i = 0; i < n; i++) {
        const int32_t u = (st.u + i * st.du) >> 16, v = (st.v + i * st.dv) >> 16;
        out[i] = px_load(s->p + v * (int32_t)s->pitch + u * (int32_t)b, b);
    }
}

/* Wrong pixels here were drawn by a filler that tests a flag the variant settles, reads a table the
   ink does not use, or walks the rows wrong: v steps back across them, a fraction a pixel. */
TEST(gfx_fill_inked_draws_as_ink)
{
    static Tables t;
    static uint8_t src[N * N * 4];
    uint32_t seed = 1;
    noise(&seed, (uint8_t *)&t, sizeof t);

    for (PxFormat f = 0; f < PX_FORMATS; f++)
        for (PxFormat g = 0; g < PX_FORMATS; g++)
            for (uint32_t v = 0; v < FILL_INKS; v++) {
                const bool lut = v & FILL_LUT;

                if (lut && (v & FILL_SOLID || f != PX_8 || g != PX_8))
                    continue; /* never a variant */

                FillInked *const inked = fill_formats[f]->inked[g][v];
                CHECK(inked);

                if (!inked)
                    continue;

                const uint32_t b = px_bytes(g);
                uint8_t want[N * 4], got[N * 4];
                noise(&seed, src, sizeof src);
                noise(&seed, want, sizeof want);
                memcpy(got, want, sizeof got);

                for (uint32_t i = 0; i < sizeof src / b;

                     i += 5) /* keyed: these, half of 32 bits' */
                    memset(src + i * b, 0, b);

                const Pixels s = {src, N * b, N, N, g};

                const Ink k = ink_of(&seed, &t, f, g, v);
                const Steps st = {0x4000, (N - 1) << 16 | 0xE000, 0xC000, -0x9000, 0};

                uint32_t texels[N];

                CHECK(fill_variant(&k, f, g) == v);
                walked(texels, &s, st, N);
                fill_formats[f]->ink(want, texels, N, &k);
                inked(got, &s, st, N, &k);
                CHECK(!memcmp(want, got, N * px_bytes(f)));
            }
}

/* Wrong pixels here were a blended shape's colour made again a pixel, or made otherwise. */
TEST(gfx_fill_tint_draws_as_ink)
{
    static Tables t;
    uint32_t seed = 2;
    noise(&seed, (uint8_t *)&t, sizeof t);

    for (PxFormat f = 0; f < PX_FORMATS; f++)
        for (uint32_t v = 0; v < FILL_INKS; v++) {
            const uint32_t blended = FILL_BLENDED | (f == PX_8 ? v & FILL_LUT : 0);
            const PxFormat source = f == PX_8 ? PX_8 : PX_32;
            const Ink k = ink_of(&seed, &t, f, source, (v & FILL_SOLID) | blended);
            const uint32_t colour = f == PX_8 ? next(&seed) & 0xFF : next(&seed) & PX_WHITE;
            uint8_t want[N * 4], got[N * 4];
            uint32_t texels[N];

            noise(&seed, want, sizeof want);
            memcpy(got, want, sizeof got);

            for (uint32_t i = 0; i < N; i++)
                texels[i] = colour;

            fill_formats[f]->ink(want, texels, N, &k);
            fill_formats[f]->tint(got, N, colour, &k);
            CHECK(!memcmp(want, got, N * px_bytes(f)));
        }
}
