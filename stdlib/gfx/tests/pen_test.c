/* The pen's numbers at 8 bits: light, tint, SOLID, a palette row, BLEND and ADD, by the indices
   drawn. Row 0 is set so each mix has one nearest index:
   0 black, 1 &H404040, 2 &H808080, 3 white, 4 red, 5 &H800000, 6 blue, 7 &H800080, 8 magenta;
   the rest green. */
#include "test.h"

static const uint32_t COLOURS[] = {
    0x000000,
    0x404040,
    0x808080,
    0xFFFFFF,
    0xFF0000,
    0x800000,
    0x0000FF,
    0x800080,
    0xFF00FF,
};

/* An 8 by 4 canvas of colour 1 in row 0 above, and a 3 by 1 sheet: 0, 3, 3. */
static int32_t canvas(Session *const S)
{
    CANVAS(8, 4);
    int32_t row[256];

    for (uint32_t i = 0; i < 256; i++)
        row[i] = i < sizeof COLOURS / sizeof *COLOURS ? (int32_t)COLOURS[i] : 0x00FF00;

    QxArray a = gfx_array(row, 256, 4);
    CHECK(!CALL("display.palette", I(0), ARRAY(&a)).error);

    const int32_t s = CALL("surface.new", I(3), I(1)).result.i;
    uint8_t texels[] = {0, 3, 3};

    QxArray b = gfx_array(texels, 3, 1);
    CALL("surface.write", I(s), I(0), I(0), I(3), I(1), ARRAY(&b));
    return s;
}

/* Light 31 halves white to &H7D7D7D, nearest &H808080; red to &H7D0000, nearest &H800000. Tint
   multiplies; SOLID draws the tint's colour; the key stays undrawn. */
TEST(gfx_pen_light_tint_and_solid)
{
    const int32_t s = canvas(S);
    CHECK(!CALL("pen.light", I(31)).error);
    CALL("draw.sprite", I(s), F(0), F(0), I(0));
    CALL("draw.rect", F(3), F(0), F(3), F(0), I(4), I(GFX_DRAW_FILL));
    ROW(0, "12251111");

    CALL("pen.reset");
    CHECK(!CALL("pen.tint", I(0x0000FF)).error);
    CALL("draw.sprite", I(s), F(0), F(1), I(0));
    CALL("draw.sprite", I(s), F(4), F(1), I(GFX_PEN_SOLID | GFX_PEN_OPAQUE));
    ROW(1, "16616661");

    CHECK(CALL("pen.light", I(64)).error == 5);
    CHECK(CALL("pen.tint", I(0x1000000)).error == 5);
    CHECK(CALL("pen.alpha", I(-1)).error == 5);
    CHECK(CALL("pen.recolour", I(16)).error == 5);
}

/* pen.recolour 2 draws index 3 as row 2's colour 3, red; the canvas's own row is untouched. */
TEST(gfx_pen_recolour_draws_through_a_row)
{
    const int32_t s = canvas(S);
    int32_t row[256] = {[3] = 0xFF0000};

    QxArray a = gfx_array(row, 256, 4);
    CALL("display.palette", I(2), ARRAY(&a));
    CHECK(!CALL("pen.recolour", I(2)).error);
    CALL("draw.sprite", I(s), F(0), F(0), I(0));
    CALL("pen.recolour", I(0));
    CALL("draw.sprite", I(s), F(4), F(0), I(0));
    ROW(0, "14411331");
}

/* Red over blue at alpha 32: (129, 0, 125), nearest &H800080; ADD at 63, magenta; alpha 0 leaves
   what is there. A quad's two triangles blend their shared edge once: twice would be nearer red. */
TEST(gfx_pen_blend_and_add)
{
    canvas(S);
    CALL("draw.clear", I(6));
    CALL("pen.alpha", I(32));
    CHECK(!CALL("draw.rect", F(0), F(0), F(1), F(0), I(4), I(GFX_DRAW_FILL | GFX_PEN_BLEND)).error);
    CALL("pen.alpha", I(63));
    CALL("draw.rect", F(2), F(0), F(3), F(0), I(4), I(GFX_DRAW_FILL | GFX_PEN_ADD));
    CALL("pen.alpha", I(0));
    CALL("draw.rect", F(4), F(0), F(5), F(0), I(4), I(GFX_DRAW_FILL | GFX_PEN_BLEND));
    ROW(0, "77886666");

    const int32_t s = CALL("surface.new", I(3), I(2)).result.i;
    gfx_draw_vec2 corners[] = {{-0.5f, 0.5f}, {7.5f, 0.5f}, {7.5f, 3.5f}, {-0.5f, 3.5f}};

    QxArray a = gfx_array(corners, 4, sizeof *corners);
    CALL("pen.alpha", I(32));
    CALL("pen.tint", I(0xFF0000));
    const int32_t flags = GFX_PEN_SOLID | GFX_PEN_OPAQUE | GFX_PEN_BLEND;
    CHECK(!CALL("draw.quad", I(s), ARRAY(&a), I(0), I(flags)).error);
    ROW(1, "77777777");
    ROW(2, "77777777");
    ROW(3, "77777777");
}

/* A quad's diagonal from (4.73, -0.17) to (0.32, 1.72) crosses row 1 at 2.00000024 walked from
   its top, 2 from its bottom: each triangle walking its own way once blended pixel (2, 1) twice,
   red over red-and-blue. Walked from the top by both, it is drawn once. */
TEST(gfx_shared_edges_blend_once)
{
    const int32_t s = canvas(S);
    CALL("draw.clear", I(6));
    gfx_draw_vec2 corners[] = {{4.73f, -0.17f}, {7.5f, 3.5f}, {0.32f, 1.72f}, {-0.5f, -0.5f}};

    QxArray a = gfx_array(corners, 4, sizeof *corners);
    CALL("pen.alpha", I(32));
    CALL("pen.tint", I(0xFF0000));
    const int32_t flags = GFX_PEN_SOLID | GFX_PEN_OPAQUE | GFX_PEN_BLEND;
    CHECK(!CALL("draw.quad", I(s), ARRAY(&a), I(0), I(flags)).error);
    CHECK(PX(2, 1) == 7);

    for (int32_t y = 0; y < 4; y++)
        for (int32_t x = 0; x < 8; x++)
            CHECK(PX(x, y) == 6 || PX(x, y) == 7);
}
