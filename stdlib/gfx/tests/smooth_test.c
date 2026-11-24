/* SMOOTH in software, as the GPU's shader: the four texels round a pixel's centre mixed, a keyed
   one adding nothing, drawn where they are mostly kept. Worked by hand: a 2 by 1 picture, blue then
   red, scaled 2 across a row of 4, puts the pixels' centres at u 0.25, 0.75, 1.25 and 1.75, so
   the mixes are all blue, three quarters blue, three quarters red, all red. */
#include "test.h"

enum { BLUE = 0x0000FF, RED = 0xFF0000, MOSTLY_BLUE = 0x4000BF, MOSTLY_RED = 0xBF0040 };

/* A 4 by h screen of depth, and a 2 by h picture of the texels given, as 32 bits. */
static int32_t picture(
    Session *const S,
    const int32_t depth,
    const int32_t h,
    const uint32_t *const texels
)
{
    gfx_fresh(S);
    CHECK(!CALL("display.open", I(4), I(h * 2), I(depth), I(0)).error);
    const int32_t s = CALL("surface.new", I(2), I(h)).result.i;

    const uint32_t bytes = depth == 8 ? 1 : 4, n = 2 * (uint32_t)h;
    uint8_t raw[16];

    for (uint32_t i = 0; i < n; i++)
        for (uint32_t b = 0; b < bytes; b++)
            raw[i * bytes + b] = (uint8_t)(texels[i] >> (8 * b));

    QxArray a = gfx_array(raw, n * bytes, 1);
    CHECK(!CALL("surface.write", I(s), I(0), I(0), I(2), I(h), ARRAY(&a)).error);
    return s;
}

/* Scaled 2 about its first texel's centre at 0.5, 0.5: its edges at -0.5 and 3.5 across. */
static void scaled(Session *const S, const int32_t s, const int32_t flags)
{
    CHECK(!CALL("draw.sprite_turned", I(SURF_SCREEN), F(0.5), F(0.5), F(0), F(2), I(s), I(flags))
               .error);
}

enum { OPAQUE_BLUE = 0xFF000000 | BLUE, OPAQUE_RED = 0xFF000000 | RED };

TEST(gfx_smooth_mixes_four_texels)
{
    const int32_t s = picture(S, 32, 1, (const uint32_t[]){OPAQUE_BLUE, OPAQUE_RED});

    CALL("draw.clear", I(SURF_SCREEN), I(0));
    scaled(S, s, GFX_DRAW_SMOOTH);
    CHECK(PX(0, 0) == BLUE && PX(1, 0) == MOSTLY_BLUE && PX(2, 0) == MOSTLY_RED && PX(3, 0) == RED);

    CALL("draw.clear", I(SURF_SCREEN), I(0));
    scaled(S, s, 0); /* without, each pixel takes the texel at its centre */
    CHECK(PX(0, 0) == BLUE && PX(1, 0) == BLUE && PX(2, 0) == RED && PX(3, 0) == RED);
}

/* A 2 by 2 picture, blue and red above two keyed: pixel 1, 1 is 9/16 blue, 3/16 red and 4/16
   keyed, so it is drawn, three quarters blue (where the nearest texel is all blue); pixel 2, 1 is
   three quarters red; pixel 1, 2 is three quarters keyed, so it is not drawn. */
TEST(gfx_smooth_keys_by_coverage)
{
    const int32_t s = picture(S, 32, 2, (const uint32_t[]){OPAQUE_BLUE, OPAQUE_RED, 0, 0});

    CALL("draw.clear", I(SURF_SCREEN), I(0x00FF00));
    scaled(S, s, GFX_DRAW_SMOOTH);
    CHECK(PX(0, 0) == BLUE && PX(1, 1) == MOSTLY_BLUE && PX(2, 1) == MOSTLY_RED);
    CHECK(PX(1, 2) == 0x00FF00 && PX(0, 3) == 0x00FF00);
}

/* At 8 bits the mix is matched to row 0: indices 1 and 2 blue and red, 3 and 4 the mixes. */
TEST(gfx_smooth_matches_8_bits_to_the_palette)
{
    const int32_t s = picture(S, 8, 1, (const uint32_t[]){1, 2});

    int32_t row[256] = {[1] = BLUE, [2] = RED, [3] = MOSTLY_BLUE, [4] = MOSTLY_RED};

    QxArray a = gfx_array(row, 256, 4);
    CALL("display.set_palette", I(0), ARRAY(&a));
    scaled(S, s, GFX_DRAW_SMOOTH);
    CHECK(PX(0, 0) == 1 && PX(1, 0) == 3 && PX(2, 0) == 4 && PX(3, 0) == 2);
}

/* Upright at 1:1 on a whole pixel, every centre is a texel's: SMOOTH changes nothing. A quarter
   pixel on, its edges at 0.75 and 2.75, the centres fall at u 0.25 and 1.25: an upright sprite is
   mixed as a turned one is. */
TEST(gfx_smooth_sprites_between_pixels)
{
    const int32_t s = picture(S, 32, 1, (const uint32_t[]){OPAQUE_BLUE, OPAQUE_RED});

    CALL("draw.clear", I(SURF_SCREEN), I(0));
    CALL("draw.sprite", I(SURF_SCREEN), F(1), F(0), I(s), I(GFX_DRAW_SMOOTH));
    CHECK(PX(0, 0) == 0 && PX(1, 0) == BLUE && PX(2, 0) == RED && PX(3, 0) == 0);

    CALL("draw.clear", I(SURF_SCREEN), I(0));
    CALL("draw.sprite", I(SURF_SCREEN), F(1.25), F(0), I(s), I(GFX_DRAW_SMOOTH));
    CHECK(PX(0, 0) == 0 && PX(1, 0) == BLUE && PX(2, 0) == MOSTLY_RED && PX(3, 0) == 0);
}
