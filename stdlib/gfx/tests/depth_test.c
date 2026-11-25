/* Canvases of 15, 16 and 32 bits: colours &HRRGGBB, packed and read back; the style's light and
   blends by arithmetic; 8-bit textures through the palette, 32-bit ones keyed by alpha; and a
   32-bit texture on an 8-bit canvas, matched to the palette. Worked by hand. */
#include "test.h"

#define POINT(x, y) PX(x, y)

static void canvas(Session *const S, const int32_t w, const int32_t depth)
{
    CHECK(!CALL("display.open", I(w), I(1), I(depth), I(0)).error);
}

/* &H123456 at 16 bits is 2, 13, 10: read back &H103452; at 15 bits 2, 6, 10, &H103152. */
TEST(gfx_depths_pack_their_colours)
{
    gfx_fresh(S);
    canvas(S, 4, 32);
    CALL("draw.clear", I(SURF_SCREEN), I(0x123456));
    CHECK(POINT(0, 0) == 0x123456 && CALL("surface.bits", I(SURF_SCREEN)).result.i == 32);
    CHECK(CALL("display.rgb", I(1), I(2), I(3)).result.i == 0x010203);
    CHECK(CALL("draw.clear", I(SURF_SCREEN), I(0x1000000)).error == 5);

    uint8_t bytes[4];
    QxArray a = gfx_array(bytes, 4, 1);
    CALL("surface.read", I(SURF_SCREEN), I(0), I(0), I(1), I(1), ARRAY(&a));
    CHECK(bytes[0] == 0x56 && bytes[1] == 0x34 && bytes[2] == 0x12 && bytes[3] == 0xFF);

    canvas(S, 4, 16);
    CALL("draw.clear", I(SURF_SCREEN), I(0x123456));
    CHECK(POINT(3, 0) == 0x103452);
    canvas(S, 4, 15);
    CALL("draw.clear", I(SURF_SCREEN), I(0x123456));
    CHECK(POINT(3, 0) == 0x103152);
}

/* Red over blue at alpha 32: (129, 0, 125), at 16 bits (132, 0, 123); ADD at 63 magenta. Light
   31 scales by 125/255: &HFF8000 to &H7D3E00. */
TEST(gfx_depths_blend_and_light)
{
    gfx_fresh(S);
    canvas(S, 4, 32);
    CALL("draw.clear", I(SURF_SCREEN), I(0x0000FF));
    CALL("draw.set_alpha", I(32));
    CALL(
        "draw.rect",
        I(SURF_SCREEN),
        F(0),
        F(0),
        F(0),
        F(0),
        I(0xFF0000),
        I(GFX_DRAW_FILL | GFX_DRAW_BLEND)
    );
    CALL("draw.set_alpha", I(63));
    CALL(
        "draw.rect",
        I(SURF_SCREEN),
        F(1),
        F(0),
        F(1),
        F(0),
        I(0xFF0000),
        I(GFX_DRAW_FILL | GFX_DRAW_ADD)
    );
    CALL("draw.reset");
    CALL("draw.set_light", I(31));
    CALL("draw.pset", I(SURF_SCREEN), F(2), F(0), I(0xFF8000));
    CHECK(POINT(0, 0) == 0x81007D && POINT(1, 0) == 0xFF00FF && POINT(2, 0) == 0x7D3E00);
    CHECK(POINT(3, 0) == 0x0000FF);

    canvas(S, 4, 16); /* the style is global: open keeps it */
    CALL("draw.reset");
    CALL("draw.clear", I(SURF_SCREEN), I(0x0000FF));
    CALL("draw.set_alpha", I(32));
    CALL(
        "draw.rect",
        I(SURF_SCREEN),
        F(0),
        F(0),
        F(0),
        F(0),
        I(0xFF0000),
        I(GFX_DRAW_FILL | GFX_DRAW_BLEND)
    );
    CHECK(POINT(0, 0) == 0x84007B);
}

/* Text's colour goes through the style as a shape's: lit, as the pset above. */
TEST(gfx_depths_light_text)
{
    gfx_fresh(S);
    canvas(S, 8, 32);
    CALL("draw.set_light", I(31));
    CHECK(
        !CALL("draw.text", I(SURF_SCREEN), F(0), F(0), STR("\xdb"), I(0xFF8000)).error
    ); /* a full cell */
    CHECK(POINT(0, 0) == 0x7D3E00 && POINT(7, 0) == 0x7D3E00);
}

/* An 8-bit texture, made before the screen, drawn through row 0 (the VGA's: 1 &H0000AA, 4
   &HAA0000), lit: &HAA times 31/63 is &H53. */
TEST(gfx_depths_draw_8_bit_textures)
{
    gfx_fresh(S);
    const int32_t s = CALL("surface.new", I(3), I(1)).result.i;
    uint8_t texels[] = {0, 1, 4};

    QxArray a = gfx_array(texels, 3, 1);
    CALL("surface.write", I(s), I(0), I(0), I(3), I(1), ARRAY(&a));
    canvas(S, 8, 32);
    CHECK(CALL("surface.bits", I(s)).result.i == 8);
    CALL("draw.clear", I(SURF_SCREEN), I(0x111111));
    CALL("draw.sprite", I(SURF_SCREEN), F(0), F(0), I(s), I(0));
    CALL("draw.set_light", I(31));
    CALL("draw.sprite", I(SURF_SCREEN), F(4), F(0), I(s), I(0));
    CHECK(POINT(0, 0) == 0x111111 && POINT(1, 0) == 0x0000AA && POINT(2, 0) == 0xAA0000);
    CHECK(POINT(4, 0) == 0x111111 && POINT(5, 0) == 0x000053 && POINT(6, 0) == 0x530000);
}

/* A 32-bit surface starts transparent: what is drawn into it is opaque, the rest keyed. */
TEST(gfx_depths_key_32_bit_textures_by_alpha)
{
    gfx_fresh(S);
    canvas(S, 8, 32);
    const int32_t s = CALL("surface.new", I(3), I(1)).result.i;
    CHECK(CALL("surface.bits", I(s)).result.i == 32);
    CALL("draw.pset", I(s), F(1), F(0), I(0x00FF00));
    CALL("draw.clear", I(SURF_SCREEN), I(0x111111));
    CALL("draw.sprite", I(SURF_SCREEN), F(0), F(0), I(s), I(0));
    CALL("draw.sprite", I(SURF_SCREEN), F(4), F(0), I(s), I(GFX_DRAW_OPAQUE));
    CHECK(POINT(0, 0) == 0x111111 && POINT(1, 0) == 0x00FF00 && POINT(2, 0) == 0x111111);
    CHECK(POINT(4, 0) == 0 && POINT(5, 0) == 0x00FF00 && POINT(6, 0) == 0);

    canvas(S, 8, 16); /* the same at 16 bits: the copy fused, 0 the key */
    const int32_t t = CALL("surface.new", I(2), I(1)).result.i;
    CALL("draw.pset", I(t), F(1), F(0), I(0xFF0000));
    CALL("draw.clear", I(SURF_SCREEN), I(0x0000FF));
    CALL("draw.sprite", I(SURF_SCREEN), F(0), F(0), I(t), I(0));
    CHECK(POINT(0, 0) == 0x0000FF && POINT(1, 0) == 0xFF0000);
}

/* A 32-bit texture on an 8-bit canvas: its colours matched to row 0 (red is index 4 here). */
TEST(gfx_depths_match_32_bits_to_the_palette)
{
    gfx_fresh(S);
    canvas(S, 4, 32);
    const int32_t s = CALL("surface.new", I(2), I(1)).result.i;
    CALL("draw.pset", I(s), F(0), F(0), I(0xFF0000));
    canvas(S, 4, 8);

    int32_t row[256] = {[1] = 0x404040, [4] = 0xFF0000, [6] = 0x0000FF};

    QxArray a = gfx_array(row, 256, 4);
    CALL("display.set_palette", I(0), ARRAY(&a));
    CALL("draw.clear", I(SURF_SCREEN), I(1));
    CHECK(!CALL("draw.sprite", I(SURF_SCREEN), F(1), F(0), I(s), I(0)).error);
    CHECK(PX(0, 0) == 1 && PX(1, 0) == 4 && PX(2, 0) == 1);
}
