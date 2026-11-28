/* Levels: each a 2 by 2 box of the last, keys by coverage, worked by hand. */
#include "core/levels.h"
#include "core/palette.h"
#include "test.h"

/* A level of the four texels given, as a 2 by 2 texture of format f: its one texel. */
static uint32_t halved(
    const PxFormat f,
    const uint32_t *const texels,
    const bool keyed,
    const uint32_t *const row
)
{
    const uint32_t b = px_bytes(f);
    uint8_t from[16] = {0}, into[4] = {0};

    for (uint32_t k = 0; k < 4; k++)
        px_store(from + k * b, texels[k], b);

    levels_halve(&(Pixels){from, 2 * b, 2, 2, f}, &(Pixels){into, b, 1, 1, f}, keyed, row);
    return px_load(into, b);
}

TEST(gfx_levels_stop_at_the_least_side)
{
    CHECK(levels_of(64, 64) == 5); /* 64, 32, 16, 8, 4 */
    CHECK(levels_of(256, 64) == 5);
    CHECK(levels_of(48, 48) == 4); /* 48, 24, 12, 6 */
    CHECK(levels_of(8, 8) == 2);
    CHECK(levels_of(7, 100) == 1);
}

TEST(gfx_levels_mean_four_texels)
{
    const uint32_t four[] = {0xFF000000, 0xFF0000FF, 0xFF00FF00, 0xFFFF0003};

    /* 0+0+0+255 over 4 rounds to 64 in each of r, g; b (255+3) / 4 rounds to 65 */
    CHECK(halved(PX_32, four, false, NULL) == 0xFF404041);
}

/* Keyed, a texel is drawn when two of its four are, as their mean; one drawn, it is the key.
   Not keyed, all four count. */
TEST(gfx_levels_key_by_coverage)
{
    const uint32_t two[] = {0xFF0000FF, 0, 0xFF00FF00, 0};
    const uint32_t one[] = {0xFF0000FF, 0, 0, 0};

    CHECK(halved(PX_32, two, true, NULL) == 0xFF008080);
    CHECK(halved(PX_32, one, true, NULL) == 0);
    CHECK(halved(PX_32, two, false, NULL) == 0x80004040);
}

/* At 16 bits black is the key: two drawn texels nearly black mean to black, so it is kept the
   smallest blue, else it would vanish from a keyed draw. Not keyed, it stays black. */
TEST(gfx_levels_keep_a_drawn_black_drawn)
{
    const uint32_t dark[] = {0x0001, 0, 0x0020, 0};

    CHECK(halved(PX_16, dark, true, NULL) == 1);
    CHECK(halved(PX_16, (const uint32_t[]){0x0001, 0, 0x0020, 0}, false, NULL) == 0);
}

/* At 8 bits the mean is of row 0's colours, matched back: index 7 is black as index 0 is, and a
   level of drawn black texels keeps 7, never 0, the key. */
TEST(gfx_levels_match_indices_past_the_key)
{
    uint32_t row[PAL_COLOURS] = {0};

    for (uint32_t i = 1; i < PAL_COLOURS; i++)
        row[i] = i == 7 ? 0 : 0x101010 * (i % 16) | 0x80;

    CHECK(halved(PX_8, (const uint32_t[]){7, 7, 0, 0}, true, row) == 7);
    CHECK(halved(PX_8, (const uint32_t[]){7, 7, 0, 0}, false, row) == 0);
}

/* A 5 by 3 texture's level is 2 by 1: its last column and row are left out. */
TEST(gfx_levels_leave_out_an_odd_edge)
{
    uint8_t from[15], into[2] = {0};

    for (uint32_t i = 0; i < 15; i++)
        from[i] = (uint8_t)i;

    uint32_t row[PAL_COLOURS];

    for (uint32_t i = 0; i < PAL_COLOURS; i++)
        row[i] = i * 0x010101;

    levels_halve(&(Pixels){from, 5, 5, 3, PX_8}, &(Pixels){into, 2, 2, 1, PX_8}, false, row);
    /* (0 + 1 + 5 + 6) / 4 = 3; (2 + 3 + 7 + 8) / 4 = 5 */
    CHECK(into[0] == 3 && into[1] == 5);
}

/* A 16 by 8 checker of black and white: one level, 8 by 4, all grey. */
TEST(gfx_levels_make_a_chain)
{
    gfx_fresh(S);
    uint32_t texels[16 * 8];

    for (uint32_t i = 0; i < 16 * 8; i++)
        texels[i] = (i + i / 16) % 2 ? 0xFFFFFFFF : 0xFF000000;

    Levels l;
    CHECK(levels_make(&l, &(Pixels){(uint8_t *)texels, 16 * 4, 16, 8, PX_32}, false, NULL));
    CHECK(l.n == 2 && l.at[1].w == 8 && l.at[1].h == 4);

    for (int32_t i = 0; i < 8 * 4; i++)
        CHECK(px_load(l.at[1].p + 4 * i, 4) == 0xFF808080);

    levels_done(&l);
}
