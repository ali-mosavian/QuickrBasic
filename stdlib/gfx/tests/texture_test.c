/* gfx.draw's turned sprites, quads and strips at 8 bits, by their pixels read. Each pixel takes
   the texel at its centre; the expectations are worked by hand, with no sample on a texel's edge
   (where backends may round either way). */
#include "base/gmath.h"
#include "soft/soft.h"
#include "test.h"

/* A 3 by 2 sheet: 0 7 9 above 2 0 3. */
static int32_t sheet(Session *const S)
{
    const int32_t s = CALL("surface.new", I(3), I(2)).result.i;
    uint8_t bytes[] = {0, 7, 9, 2, 0, 3};

    QxArray a = gfx_array(bytes, 6, 1);
    CALL("surface.write", I(s), I(0), I(0), I(3), I(2), ARRAY(&a));
    return s;
}

/* A quarter turn clockwise about the anchor: texel (i, j) lands at (3 - j, 3 + i). */
TEST(gfx_sprite_turned_a_quarter)
{
    CANVAS(8, 8);
    const int32_t s = sheet(S);
    CHECK(!CALL("draw.sprite_turned", I(s), F(3), F(3), F(GM_PI / 2), F(1), I(0)).error);
    ROW(2, "11111111");
    ROW(3, "11211111");
    ROW(4, "11171111");
    ROW(5, "11391111");
    ROW(6, "11111111");
}

/* Twice the size, its edges at 1.25 and 7.25 across, 1.25 and 5.25 down: a texel two pixels
   square. FLIPX mirrors it. */
TEST(gfx_sprite_turned_scaled_and_flipped)
{
    CANVAS(8, 8);
    const int32_t s = sheet(S);
    CALL("draw.sprite_turned", I(s), F(2.25), F(2.25), F(0), F(2), I(0));
    ROW(1, "11111111");
    ROW(2, "11117799");
    ROW(3, "11117799");
    ROW(4, "11221133");
    ROW(5, "11221133");
    ROW(6, "11111111");

    gfx_display_command c;
    CALL("display.drawn", I(2), REF(&c));
    CHECK(c.kind == GFX_DISPLAY_DREW_SPRITE && c.scale == 2 && c.source == s);

    CANVAS(8, 8);
    const int32_t t = sheet(S);
    CALL("draw.sprite_turned", I(t), F(2.25), F(2.25), F(0), F(2), I(GFX_DRAW_FLIPX));
    ROW(2, "11997711");
    ROW(4, "11331122");
}

/* A quad over the sprite's own square draws it as the sprite does: its two triangles meet with
   no pixel missed. */
TEST(gfx_quad_over_the_sprite)
{
    gfx_draw_vec2 corners[] = {{-0.5f, -0.5f}, {2.5f, -0.5f}, {2.5f, 1.5f}, {-0.5f, 1.5f}};

    QxArray a = gfx_array(corners, 4, sizeof *corners);

    CANVAS(8, 4);
    const int32_t s = sheet(S);
    CHECK(!CALL("draw.quad", I(s), ARRAY(&a), I(0), I(0)).error);
    ROW(0, "17911111");
    ROW(1, "21311111");
    CALL("draw.quad", I(s), ARRAY(&a), I(0), I(GFX_PEN_OPAQUE));
    ROW(0, "07911111");
    ROW(1, "20311111");
    CHECK(CALL("draw.quad", I(s), ARRAY(&a), I(1), I(0)).error == 9);
}

/* A 2 by 2 picture, 5 6 above 7 8, along (0.5, 1.5), (4.5, 1.5), (8.5, 3.5): its top edge the
   line, u running with x and repeating, its texels upright down the slope. */
TEST(gfx_strip_follows_the_points)
{
    gfx_draw_vec2 points[] = {{0.5f, 1.5f}, {4.5f, 1.5f}, {8.5f, 3.5f}};

    QxArray a = gfx_array(points, 3, sizeof *points);

    CANVAS(8, 6);
    const int32_t s = CALL("surface.new", I(2), I(2)).result.i;
    uint8_t bytes[] = {5, 6, 7, 8};

    QxArray b = gfx_array(bytes, 4, 1);
    CALL("surface.write", I(s), I(0), I(0), I(2), I(2), ARRAY(&b));
    CHECK(!CALL("draw.strip", I(s), ARRAY(&a), I(0), I(3), I(0)).error);
    ROW(1, "11111111");
    ROW(2, "15656511");
    ROW(3, "17878765");
    ROW(4, "11111187");
    ROW(5, "11111111");
    CHECK(CALL("draw.strip", I(s), ARRAY(&a), I(0), I(1), I(0)).error == 5);
}

/* Under a camera a quarter turn clockwise, an upright sprite turns a quarter back: the world's
   (x, y) at (y + 3.3, 3.8 - x), so the anchor at (-1.2, -0.3) lands on (3, 5) and texel (i, j)
   at (3 + j, 5 - i). */
TEST(gfx_sprite_under_a_turned_camera)
{
    CANVAS(8, 8);
    const int32_t s = sheet(S);
    CALL("draw.camera_turned", F(0.3), F(0.2), F(GM_PI / 2), F(1));
    CHECK(!CALL("draw.sprite", I(s), F(-1.2), F(-0.3), I(0)).error);
    ROW(2, "11111111");
    ROW(3, "11193111");
    ROW(4, "11171111");
    ROW(5, "11112111");
    ROW(6, "11111111");
}

/* The sheet as a font of one glyph, ' ': in a colour, its texels' shape; AS_DRAWN, the texels. A
   character with no glyph takes its width. */
TEST(gfx_text_in_a_sheet_font)
{
    CANVAS(8, 8);
    const int32_t s = sheet(S);
    const int32_t f = CALL("pen.new_font", I(s), I(3), I(2)).result.i;
    CHECK(f && !CALL("pen.font", I(f)).error);
    CHECK(CALL("draw.text_width", STR("ab")).result.i == 6);
    CHECK(!CALL("draw.text", F(1), F(0), STR("! "), I(4)).error);
    ROW(0, "11111441");
    ROW(1, "11114141");
    CALL("draw.text", F(1), F(2), STR(" "), I(GFX_DRAW_AS_DRAWN));
    ROW(2, "11791111");
    ROW(3, "12131111");

    CALL("pen.font", I(0));
    CHECK(CALL("draw.text_width", STR("ab")).result.i == 16);
}

/* Under gfx_sprite_under_a_turned_camera's quarter turn, text lands where the sprite does. */
TEST(gfx_text_turns_with_the_camera)
{
    CANVAS(8, 8);
    const int32_t s = sheet(S);
    CALL("pen.font", I(CALL("pen.new_font", I(s), I(3), I(2)).result.i));
    CALL("draw.camera_turned", F(0.3), F(0.2), F(GM_PI / 2), F(1));
    CHECK(!CALL("draw.text", F(-1.2), F(-0.3), STR(" "), I(4)).error);
    ROW(3, "11144111");
    ROW(4, "11141111");
    ROW(5, "11114111");
}

TEST(gfx_fonts_refuse)
{
    CANVAS(8, 8);
    const int32_t s = sheet(S);
    CHECK(CALL("pen.new_font", I(s), I(0), I(1)).error == 5);
    CHECK(CALL("pen.new_font", I(s), I(4), I(1)).error == 5);
    CHECK(CALL("pen.new_font", I(0x10000077), I(1), I(1)).error == 5);
    CHECK(CALL("pen.font", I(0x20000077)).error == 5);
    CHECK(CALL("pen.font", I(s)).error == 5);

    CALL("pen.font", I(CALL("pen.new_font", I(s), I(1), I(1)).result.i));
    CALL("surface.free", I(s));
    CHECK(CALL("draw.text", F(0), F(0), STR("!"), I(4)).error == 5);
    CHECK(PX(0, 0) == 1);
}

/* qgl's perspective scan of a trapezoid, (2, 0) (6, 0) (8, 4) (0, 4) with 1/z 1 down to 2 and
   u/z, v/z linear (u 0 to 8 across, v 0 to 4 down), over an 8 by 4 picture whose texel (u, v) is
   10 + 8v + u. Worked from qgl's rule: edges evaluated a row below the vertex (x at rows 0 to 3:
   1.5..6.5, 1..7, 0.5..7.5, 0..8, so spans of 5, 6, 7 and 8 pixels), u/z and v/z at the first
   pixel, u = (u/z) / (1/z) in 16.16 rounded, plus half a texel, and the far end a whole run (16
   pixels) on, unclamped; the steps are the difference over 16, and each texel is held inside the
   picture. Row 1: u 1.83 stepping 1.33, v 3.17, texels 35 37 38 39 41 and 41 (8.5 held to 7).
   It once clamped the far end to the picture, as qgl's bound mode: a short span's step bent,
   row 0 read 27 27 28 28 28 where u runs 1.3 to 7.7. */
TEST(gfx_soft_perspective_trapezoid)
{
    uint8_t tex[32], dst[16 * 8];

    for (uint32_t i = 0; i < 32; i++)
        tex[i] = (uint8_t)(10 + i);

    memset(dst, 1, sizeof dst);

    const SoftTarget t = {
        .px = {dst, 16, 16, 8, PX_8},
        .clip = {0, 0, 16, 8},
        .fill = &fill8,
    };

    const Pixels s = {tex, 8, 8, 4, PX_8};

    const ScanVert v[] = {{2, 0, 1, 0, 0}, {6, 0, 1, 8, 0}, {8, 4, 2, 16, 8}, {0, 4, 2, 0, 8}};

    soft_texture_persp(&t, &s, v, 4, GFX_PEN_OPAQUE, false);

    static const uint8_t want[8][16] = {
        {1, 27, 28, 30, 32, 33, 1, 1},
        {1, 35, 37, 38, 39, 41, 41, 1},
        {35, 36, 37, 38, 39, 40, 41, 1},
        {35, 36, 37, 38, 39, 40, 41, 41},
    };

    for (uint32_t y = 0; y < 8; y++)
        for (uint32_t x = 0; x < 16; x++)
            CHECK(dst[y * 16 + x] == (y < 4 && x < 8 ? want[y][x] : 1));
}
