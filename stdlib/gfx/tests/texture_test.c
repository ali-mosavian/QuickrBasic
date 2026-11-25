/* gfx.draw's turned sprites, quads and strips at 8 bits, by their pixels read. Each pixel takes
   the texel at its centre; the expectations are worked by hand, with no sample on a texel's edge
   (where backends may round either way). */
#include "base/gmath.h"
#include "backend/soft/soft.h"
#include "backend/soft/span.h"
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
    CHECK(!CALL("draw.sprite_turned", I(SURF_SCREEN), F(3), F(3), F(GM_PI / 2), F(1), I(s), I(0))
               .error);
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
    CALL("draw.sprite_turned", I(SURF_SCREEN), F(2.25), F(2.25), F(0), F(2), I(s), I(0));
    ROW(1, "11111111");
    ROW(2, "11117799");
    ROW(3, "11117799");
    ROW(4, "11221133");
    ROW(5, "11221133");
    ROW(6, "11111111");

    gfx_display_command c;
    CALL("display.get_drawn", I(2), REF(&c));
    CHECK(c.kind == GFX_DISPLAY_DREW_SPRITE && c.scale == 2 && c.source == s);

    CANVAS(8, 8);
    const int32_t t = sheet(S);
    CALL(
        "draw.sprite_turned",
        I(SURF_SCREEN),
        F(2.25),
        F(2.25),
        F(0),
        F(2),
        I(t),
        I(GFX_DRAW_FLIP_X)
    );
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
    CHECK(!CALL("draw.quad", I(SURF_SCREEN), ARRAY(&a), I(0), I(s), I(0)).error);
    ROW(0, "17911111");
    ROW(1, "21311111");
    CALL("draw.quad", I(SURF_SCREEN), ARRAY(&a), I(0), I(s), I(GFX_DRAW_OPAQUE));
    ROW(0, "07911111");
    ROW(1, "20311111");
    CHECK(CALL("draw.quad", I(SURF_SCREEN), ARRAY(&a), I(1), I(s), I(0)).error == 9);
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
    CHECK(!CALL("draw.strip", I(SURF_SCREEN), ARRAY(&a), I(0), I(3), I(s), I(0)).error);
    ROW(1, "11111111");
    ROW(2, "15656511");
    ROW(3, "17878765");
    ROW(4, "11111187");
    ROW(5, "11111111");
    CHECK(CALL("draw.strip", I(SURF_SCREEN), ARRAY(&a), I(0), I(1), I(s), I(0)).error == 5);
}

/* Under a camera a quarter turn clockwise, an upright sprite turns a quarter back: the world's
   (x, y) at (y + 3.3, 3.8 - x), so the anchor at (-1.2, -0.3) lands on (3, 5) and texel (i, j)
   at (3 + j, 5 - i). */
TEST(gfx_sprite_under_a_turned_camera)
{
    CANVAS(8, 8);
    const int32_t s = sheet(S);
    CALL("surface.set_camera_turned", I(SURF_SCREEN), F(0.3), F(0.2), F(GM_PI / 2), F(1));
    CHECK(!CALL("draw.sprite", I(SURF_SCREEN), F(-1.2), F(-0.3), I(s), I(0)).error);
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
    const int32_t f = CALL("draw.new_font", I(s), I(3), I(2)).result.i;
    CHECK(f && !CALL("draw.set_font", I(f)).error);
    CHECK(CALL("draw.text_width", STR("ab")).result.i == 6);
    CHECK(!CALL("draw.text", I(SURF_SCREEN), F(1), F(0), STR("! "), I(4)).error);
    ROW(0, "11111441");
    ROW(1, "11114141");
    CALL("draw.text", I(SURF_SCREEN), F(1), F(2), STR(" "), I(GFX_DRAW_AS_DRAWN));
    ROW(2, "11791111");
    ROW(3, "12131111");

    CALL("draw.set_font", I(0));
    CHECK(CALL("draw.text_width", STR("ab")).result.i == 16);
}

/* Under gfx_sprite_under_a_turned_camera's quarter turn, text lands where the sprite does. */
TEST(gfx_text_turns_with_the_camera)
{
    CANVAS(8, 8);
    const int32_t s = sheet(S);
    CALL("draw.set_font", I(CALL("draw.new_font", I(s), I(3), I(2)).result.i));
    CALL("surface.set_camera_turned", I(SURF_SCREEN), F(0.3), F(0.2), F(GM_PI / 2), F(1));
    CHECK(!CALL("draw.text", I(SURF_SCREEN), F(-1.2), F(-0.3), STR(" "), I(4)).error);
    ROW(3, "11144111");
    ROW(4, "11141111");
    ROW(5, "11114111");
}

TEST(gfx_fonts_refuse)
{
    CANVAS(8, 8);
    const int32_t s = sheet(S);
    CHECK(CALL("draw.new_font", I(s), I(0), I(1)).error == 5);
    CHECK(CALL("draw.new_font", I(s), I(4), I(1)).error == 5);
    CHECK(CALL("draw.new_font", I(0x10000077), I(1), I(1)).error == 5);
    CHECK(CALL("draw.set_font", I(0x20000077)).error == 5);
    CHECK(CALL("draw.set_font", I(s)).error == 5);

    CALL("draw.set_font", I(CALL("draw.new_font", I(s), I(1), I(1)).result.i));
    CALL("surface.free", I(s));
    CHECK(CALL("draw.text", I(SURF_SCREEN), F(0), F(0), STR("!"), I(4)).error == 5);
    CHECK(PX(0, 0) == 1);
}

/* A trapezoid in perspective, pixel centres (1.5, -0.5) (5.5, -0.5) (7.5, 3.5) (-0.5, 3.5), z 1 down
   to 0.5, over an 8 by 4 picture whose texel (u, v) is 10 + 8v + u, u 0.5 to 8.5 across and v 0.5
   to 4.5 down. Worked by hand from the plane (1/z, u/z, v/z linear): rows 0 to 3 cover pixels 2-5,
   1-6, 1-6 and 0-7; a span's u is taken at its first centre and a whole run (16 pixels) on,
   unclamped, the step their difference over 16, each texel held inside. Row 0's u runs 1.83 to
   7.17 by 1.78: texels 1, 3, 5, 7. Clamping the far end to the picture, as qgl's bound mode, bent
   that step to 0.39: 1, 2, 2, 2. */
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

    const Corner c[] = {
        {1.5f, -0.5f, 1, 0.5f, 0.5f},
        {5.5f, -0.5f, 1, 8.5f, 0.5f},
        {7.5f, 3.5f, 0.5f, 8.5f, 4.5f},
        {-0.5f, 3.5f, 0.5f, 0.5f, 4.5f},
    };

    soft_poly(&t, &s, c, 4, 0, GFX_DRAW_OPAQUE | GFX_DRAW_PERSPECTIVE, false);

    static const uint8_t want[8][16] = {
        {1, 1, 19, 21, 23, 25, 1, 1},
        {1, 26, 28, 29, 31, 32, 33, 1},
        {1, 35, 36, 37, 39, 40, 41, 1},
        {34, 35, 36, 37, 39, 40, 41, 41},
    };

    for (uint32_t y = 0; y < 8; y++)
        for (uint32_t x = 0; x < 16; x++)
            CHECK(dst[y * 16 + x] == (y < 4 && x < 8 ? want[y][x] : 1));
}

/* Walks past a picture's edges, both ways, each texel held inside as steps_texel holds it: the
   filler draws them in pieces, a coordinate held past its edge. Wrong pixels here were a piece cut
   at the wrong pixel, a coordinate held at the wrong edge, or held while inside. */
TEST(gfx_soft_walks_held_inside)
{
    enum { W = 8, H = 4, N = 48 };

    uint8_t tex[W * H], dst[N];

    for (uint32_t i = 0; i < W * H; i++)
        tex[i] = (uint8_t)(10 + i);

    const SoftTarget t = {.px = {dst, N, N, 1, PX_8}, .clip = {0, 0, N, 1}, .fill = &fill8};

    const Pixels s = {tex, W, W, H, PX_8};

    const Textured x = {.t = &t, .s = &s, .opaque = true};

    static const Steps walks[] = {
        {-3 * 65536, -2 * 65536, 0x5800, 0x2400, 0},
        {(W + 2) << 16, (H + 1) << 16, -0x6000, -0x1C00, 0},
        {0x8000, (H + 3) << 16, 0x3000, 0, 0},
        {-0x10000, 0x8000, 0, 0x1800, 0},
    };

    for (uint32_t k = 0; k < sizeof walks / sizeof *walks; k++) {
        const Steps st = walks[k];
        memset(dst, 0, sizeof dst);
        span_texels(&x, 0, 0, N, st, 0, 0);

        for (int32_t i = 0; i < N; i++) {
            const int32_t u = steps_held((st.u + i * st.du) >> 16, W);
            const int32_t v = steps_held((st.v + i * st.dv) >> 16, H);
            CHECK(dst[i] == tex[v * W + u]);
        }
    }
}
