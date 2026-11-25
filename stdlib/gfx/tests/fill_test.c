/* The fillers against their definitions, every one of every format's table: random texels, tables
   and pixels there; each from four starts, an odd count of pixels, so words of them are drawn
   between pixels drawn alone. */
#include <string.h>

#include "backend/soft/fill.h"
#include "unit.h"

enum { N = 64, COUNT = N - 5, STARTS = 4 };

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

/* n texels of format f at p, every fifth keyed (half of 32 bits' are), the rest random. */
static void texels_at(uint32_t *const seed, uint8_t *const p, const uint32_t n, const PxFormat f)
{
    const uint32_t b = px_bytes(f);
    noise(seed, p, n * b);

    for (uint32_t i = 0; i < n; i += 5)
        memset(p + i * b, 0, b);
}

/* An 8-bit target's lut, map and blend, a direct one's argb, all random; the ink's pixel table. */
typedef struct {
    uint8_t lut[256], map[32768], blend[65536];
    uint32_t argb[256], pixels[PX_TABLE];
} Tables;

/* The ink of variant v from g into f over the tables, its pixel table made of them. */
static Ink ink_of(
    uint32_t *const seed,
    Tables *const t,
    const PxFormat f,
    const PxFormat g,
    const uint32_t v
)
{
    const bool eight = f == PX_8, blended = v & FILL_BLENDED;
    const uint32_t shade = next(seed) & PX_WHITE;

    if (!eight || g != PX_8)
        px_table(t->pixels, g, f, t->argb, shade);

    return (Ink){
        .source = g,
        .keyed = v & FILL_KEYED,
        .solid = v & FILL_SOLID,
        .colour = eight ? next(seed) & 0xFF : PX_OPAQUE | (next(seed) & PX_WHITE),
        .lut = v & FILL_LUT ? t->lut : nullptr,
        .map = t->map,
        .blend = eight && blended ? t->blend : nullptr,
        .argb = t->argb,
        .pixels = t->pixels,
        .shade = shade,
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

/* n texels of format f into d, those keyed skipped when key. */
static void stored(
    uint8_t *const d,
    const uint32_t *const texels,
    const int32_t n,
    const PxFormat f,
    const bool key
)
{
    for (int32_t i = 0; i < n; i++)
        if (!key || !px_key(f, texels[i]))
            px_store(d + i * (int32_t)px_bytes(f), texels[i], px_bytes(f));
}

/* Two copies of random pixels there: want and got. */
typedef struct {
    uint8_t want[N * 4], got[N * 4];
} There;

static void there_of(uint32_t *const seed, There *const t)
{
    noise(seed, t->want, sizeof t->want);
    memcpy(t->got, t->want, sizeof t->got);
}

/* The walk of COUNT pixels inside an N by N picture: u and v both step, v back across the rows, a
   fraction a pixel. */
static const Steps WALK = {0x4000, (N - 1) << 16 | 0xE000, 0xC000, -0x9000, 0};

/* Wrong pixels here were drawn by a filler that tests a flag the variant settles, reads a table the
   ink does not use, walks the rows wrong, or misplaces a pixel in a word or the pixels round
   it. */
TEST(gfx_fill_inked_draws_as_ink)
{
    static Tables t;
    static uint8_t src[N * N * 4];
    uint32_t seed = 1;
    noise(&seed, (uint8_t *)&t, sizeof t);

    for (PxFormat f = 0; f < PX_FORMATS; f++)
        for (PxFormat g = 0; g < PX_FORMATS; g++)
            for (uint32_t v = 0; v < FILL_INKS; v++) {
                if (v & FILL_LUT && (v & FILL_SOLID || f != PX_8 || g != PX_8))
                    continue; /* never a variant */

                FillInked *const inked = fill_formats[f]->inked[g][v];
                CHECK(inked);

                for (uint32_t at = 0; inked && at < STARTS; at++) {
                    const uint32_t b = px_bytes(f);
                    const Pixels s = {src, N * px_bytes(g), N, N, g};

                    const Ink k = ink_of(&seed, &t, f, g, v);
                    uint32_t texels[N];
                    There d;

                    texels_at(&seed, src, N * N, g);
                    there_of(&seed, &d);
                    CHECK(fill_variant(&k, f, g) == v);
                    walked(texels, &s, WALK, COUNT);
                    fill_formats[f]->ink(d.want + at * b, texels, COUNT, &k);
                    inked(d.got + at * b, &s, WALK, COUNT, &k);
                    CHECK(!memcmp(d.want, d.got, sizeof d.want));
                }
            }
}

/* Wrong pixels here were a blended shape's colour made again a pixel, made otherwise, or put in a
   word wrong. */
TEST(gfx_fill_tint_draws_as_ink)
{
    static Tables t;
    static FillTints tints;
    uint32_t seed = 2;
    noise(&seed, (uint8_t *)&t, sizeof t);

    for (PxFormat f = 0; f < PX_FORMATS; f++)
        for (uint32_t v = 0; v < FILL_INKS * STARTS; v++) {
            const uint32_t at = v / FILL_INKS, b = px_bytes(f);
            const uint32_t blended = FILL_BLENDED | (f == PX_8 ? v & FILL_LUT : 0);
            const PxFormat source = f == PX_8 ? PX_8 : PX_32;
            const Ink k = ink_of(&seed, &t, f, source, (v & FILL_SOLID) | blended);
            const uint32_t colour = f == PX_8 ? next(&seed) & 0xFF : next(&seed) & PX_WHITE;
            uint32_t texels[N];
            There d;

            there_of(&seed, &d);

            for (uint32_t i = 0; i < N; i++)
                texels[i] = colour;

            fill_formats[f]->ink(d.want + at * b, texels, COUNT, &k);
            fill_formats[f]->tint(d.got + at * b, COUNT, colour, &k, &tints);
            CHECK(!memcmp(d.want, d.got, sizeof d.want));
        }
}

/* Wrong pixels here were a row's texels read a word at a time from the wrong place or in the wrong
   order, or keyed ones drawn. */
TEST(gfx_fill_rows_copy_their_texels)
{
    uint32_t seed = 3;
    uint8_t src[N * 4];

    for (PxFormat f = 0; f < PX_FORMATS; f++)
        for (uint32_t variant = 0; variant < FILL_VARIANTS * STARTS; variant++) {
            const uint32_t at = variant / FILL_VARIANTS, b = px_bytes(f);
            const bool key = !(variant & FILL_OPAQUE), back = variant & FILL_FLIPX;
            uint32_t texels[N];
            There d;

            texels_at(&seed, src, N, f);
            there_of(&seed, &d);

            const uint8_t *const first = src + (back ? COUNT - 1 + at : at) * b;

            for (int32_t i = 0; i < COUNT; i++)
                texels[i] = px_load(first + (back ? -i : i) * (int32_t)b, b);

            stored(d.want + at * b, texels, COUNT, f, key);
            fill_formats[f]->row[variant % FILL_VARIANTS](d.got + at * b, first, COUNT);
            CHECK(!memcmp(d.want, d.got, sizeof d.want));
        }
}

/* Wrong pixels here were a walk's texels put in a word wrong, or keyed ones drawn. */
TEST(gfx_fill_steps_draw_their_walk)
{
    static uint8_t src[N * N * 4];
    uint32_t seed = 4;

    for (PxFormat f = 0; f < PX_FORMATS; f++)
        for (uint32_t i = 0; i < 2 * STARTS; i++) {
            const uint32_t at = i / 2, b = px_bytes(f);
            const bool key = i & 1;
            const Pixels s = {src, N * b, N, N, f};

            uint32_t texels[N];
            There d;

            texels_at(&seed, src, N * N, f);
            there_of(&seed, &d);
            walked(texels, &s, WALK, COUNT);
            stored(d.want + at * b, texels, COUNT, f, key);
            fill_formats[f]->step[key ? 0 : FILL_OPAQUE](d.got + at * b, &s, WALK, COUNT);
            CHECK(!memcmp(d.want, d.got, sizeof d.want));
        }
}

/* Wrong pixels here were a span's colour put in a word wrong, or past its ends. */
TEST(gfx_fill_spans_draw_their_colour)
{
    uint32_t seed = 5;

    for (PxFormat f = 0; f < PX_FORMATS; f++)
        for (uint32_t at = 0; at < STARTS; at++) {
            const uint32_t b = px_bytes(f);
            const uint32_t c = next(&seed) & (b == 4 ? ~0u : (1u << 8 * b) - 1);
            uint32_t texels[N];
            There d;

            there_of(&seed, &d);

            for (uint32_t i = 0; i < N; i++)
                texels[i] = c;

            stored(d.want + at * b, texels, COUNT, f, false);
            fill_formats[f]->span(d.got + at * b, COUNT, c);
            CHECK(!memcmp(d.want, d.got, sizeof d.want));
        }
}
