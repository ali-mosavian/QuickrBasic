/* MIPMAP in software: a texture drawn smaller than its texels reads the level nearest its scale.
   A checker of single texels, black and white, is grey from its first level on (levels_test), so
   a pixel that reads a level is grey and one that reads the texture is black or white. */
#include "test.h"

enum { GREY = 0x808080, CHECKER = 64 };

/* A side by side texture of format's bytes, texel i bytes[i * b...]. */
static int32_t written(
    Session *const S,
    const int32_t side,
    const uint8_t *const bytes,
    const uint32_t b
)
{
    const int32_t s = CALL("surface.new", I(side), I(side)).result.i;
    QxArray a = gfx_array((void *)bytes, (uint32_t)(side * side) * b, 1);

    CHECK(!CALL("surface.write", I(s), I(0), I(0), I(side), I(side), ARRAY(&a)).error);
    return s;
}

/* A 32-bit checker, CHECKER a side, of opaque black and white. */
static int32_t checker(Session *const S)
{
    static uint32_t texels[CHECKER * CHECKER];

    for (uint32_t i = 0; i < CHECKER * CHECKER; i++)
        texels[i] = (i + i / CHECKER) % 2 ? 0xFFFFFFFF : 0xFF000000;

    return written(S, CHECKER, (const uint8_t *)texels, 4);
}

static void poly(Session *const S, const int32_t s, gfx_draw_vertex *const p, const int32_t flags)
{
    QxArray a = gfx_array(p, 4, sizeof *p);
    CHECK(!CALL("draw.poly", I(SURF_SCREEN), ARRAY(&a), I(0), I(4), I(s), I(0), I(flags)).error);
}

/* The square of pixels 0 to side - 1, the corners half a pixel out, texels 0 to texels. */
static void square(
    Session *const S,
    const int32_t s,
    const float side,
    const float texels,
    const int32_t flags
)
{
    gfx_draw_vertex p[4] = {
        {-0.5f, -0.5f, 1, 0, 0},
        {side - 0.5f, -0.5f, 1, texels, 0},
        {side - 0.5f, side - 0.5f, 1, texels, texels},
        {-0.5f, side - 0.5f, 1, 0, texels},
    };
    poly(S, s, p, flags);
}

/* Every pixel of the w by h screen want; else, with none, each black or white. */
static bool all(Session *const S, const int32_t w, const int32_t h, const int32_t want)
{
    for (int32_t y = 0; y < h; y++)
        for (int32_t x = 0; x < w; x++) {
            const int32_t c = PX(x, y);

            if (want >= 0 ? c != want : c != 0 && c != 0xFFFFFF)
                return false;
        }

    return true;
}

/* Four texels a pixel: level 2, all grey; without MIPMAP, the texels skipped, black and white. */
TEST(gfx_mipmap_minified_reads_a_level)
{
    gfx_fresh(S);
    CHECK(!CALL("display.open", I(16), I(16), I(32), I(0)).error);
    const int32_t s = checker(S);

    square(S, s, 16, CHECKER, GFX_DRAW_OPAQUE | GFX_DRAW_MIPMAP);
    CHECK(all(S, 16, 16, GREY));
    square(S, s, 16, CHECKER, GFX_DRAW_OPAQUE);
    CHECK(all(S, 16, 16, -1));
}

/* A floor in perspective: the near row a texel a pixel, level 0's black and white; the far row
   over ten texels a pixel, a level's grey. u/z is x + 1/2 all over, 1/z 1 near and 1/16 far. */
TEST(gfx_mipmap_perspective_a_level_a_row)
{
    gfx_fresh(S);
    CHECK(!CALL("display.open", I(32), I(16), I(32), I(0)).error);
    const int32_t s = checker(S);
    gfx_draw_vertex p[4] = {
        {-0.5f, -0.5f, 16, 0, 0},
        {31.5f, -0.5f, 16, 512, 0},
        {31.5f, 15.5f, 1, 32, 64},
        {-0.5f, 15.5f, 1, 0, 64},
    };

    poly(S, s, p, GFX_DRAW_OPAQUE | GFX_DRAW_PERSPECTIVE | GFX_DRAW_MIPMAP);

    bool black = false, white = false;

    for (int32_t x = 0; x < 32; x++) {
        CHECK(PX(x, 0) == GREY);
        black |= PX(x, 15) == 0;
        white |= PX(x, 15) == 0xFFFFFF;
    }

    CHECK(black && white);
}

/* The texture drawn into after its levels were made: they are made again, not the old grey. */
TEST(gfx_mipmap_levels_follow_the_texels)
{
    gfx_fresh(S);
    CHECK(!CALL("display.open", I(16), I(16), I(32), I(0)).error);
    const int32_t s = checker(S);

    square(S, s, 16, CHECKER, GFX_DRAW_OPAQUE | GFX_DRAW_MIPMAP);
    CHECK(all(S, 16, 16, GREY));
    CHECK(!CALL("draw.clear", I(s), I(0xFF0000)).error);
    square(S, s, 16, CHECKER, GFX_DRAW_OPAQUE | GFX_DRAW_MIPMAP);
    CHECK(all(S, 16, 16, 0xFF0000));
}

/* At 8 bits a level is matched in row 0: a checker of 1 and 2 means grey, index 3; row 0 changed
   so that 3 is red and 4 grey, the level is made again and reads 4. */
TEST(gfx_mipmap_indices_follow_row_0)
{
    gfx_fresh(S);
    CHECK(!CALL("display.open", I(4), I(4), I(8), I(0)).error);

    int32_t row[256] = {[1] = 0x101010, [2] = 0xF0F0F0, [3] = GREY};
    QxArray a = gfx_array(row, 256, 4);
    CALL("display.set_palette", I(0), ARRAY(&a));

    uint8_t texels[8 * 8];

    for (uint32_t i = 0; i < 8 * 8; i++)
        texels[i] = (i + i / 8) % 2 ? 2 : 1;

    const int32_t s = written(S, 8, texels, 1);

    square(S, s, 4, 8, GFX_DRAW_MIPMAP);
    CHECK(PX(0, 0) == 3 && PX(3, 3) == 3);
    row[3] = 0xFF0000;
    row[4] = GREY;
    CALL("display.set_palette", I(0), ARRAY(&a));
    square(S, s, 4, 8, GFX_DRAW_MIPMAP);
    CHECK(PX(0, 0) == 4 && PX(3, 3) == 4);
}
