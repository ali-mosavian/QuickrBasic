/* gfx.draw.poly's textures, perspective and depth at 8 bits, by their pixels read. A point at
   distance z has depth 65535 / z, nearer larger; ZTEST hides a pixel whose depth there is already
   as near. Worked by hand. */
#include "test.h"

/* Pixels x0 to x1 of rows 0 and 1 at distance z, the corners half a pixel out; texels 0 to w
   across, 0 to 1 down. */
static void band(
    gfx_draw_vertex *const p,
    const float x0,
    const float x1,
    const float z,
    const float w
)
{
    p[0] = (gfx_draw_vertex){x0 - 0.5f, -0.5f, z, 0, 0};

    p[1] = (gfx_draw_vertex){x1 + 0.5f, -0.5f, z, w, 0};

    p[2] = (gfx_draw_vertex){x1 + 0.5f, 1.5f, z, w, 1};

    p[3] = (gfx_draw_vertex){x0 - 0.5f, 1.5f, z, 0, 1};
}

static int32_t poly(
    Session *const S,
    const int32_t s,
    gfx_draw_vertex *const p,
    const int32_t n,
    const int32_t colour,
    const int32_t flags
)
{
    QxArray a = gfx_array(p, (uint32_t)n, sizeof *p);
    return CALL("draw.poly", I(s), ARRAY(&a), I(0), I(n), I(colour), I(flags)).error;
}

static int32_t flat(
    Session *const S,
    const float x0,
    const float x1,
    const float z,
    const int32_t colour,
    const int32_t flags
)
{
    gfx_draw_vertex p[4];
    band(p, x0, x1, z, 1);
    return poly(S, 0, p, 4, colour, GFX_DRAW_FILL | flags);
}

/* 65535 / 2 is 32767 and / 4 16383: the nearer hides the farther, either way round; an equal
   depth hides too. ZTEST alone leaves the depth; ZSET alone draws whatever is there. */
TEST(gfx_poly_depth_tests_and_sets)
{
    enum { BOTH = GFX_DRAW_ZTEST | GFX_DRAW_ZSET };

    CANVAS(8, 2);
    CALL("draw.clear_depth");
    CHECK(!flat(S, 0, 3, 2, 5, BOTH));
    flat(S, 0, 5, 4, 6, BOTH);
    ROW(0, "55556611");
    ROW(1, "55556611");

    flat(S, 2, 7, 2, 7, GFX_DRAW_ZTEST);
    ROW(0, "55557777");

    flat(S, 4, 7, 3, 3, GFX_DRAW_ZTEST);
    ROW(1, "55553333");

    flat(S, 0, 1, 8, 2, GFX_DRAW_ZSET);
    flat(S, 0, 3, 4, 4, GFX_DRAW_ZTEST);
    ROW(0, "44553333");

    CALL("draw.clear_depth");
    flat(S, 0, 7, 9, 8, BOTH);
    ROW(1, "88888888");
}

/* Texels 0 and 7, two pixels each: the keyed texel draws nothing and sets no depth, so a farther
   flat band shows through it alone. */
TEST(gfx_poly_keyed_texels_set_no_depth)
{
    CANVAS(4, 2);
    const int32_t s = CALL("surface.new", I(2), I(1)).result.i;
    uint8_t bytes[] = {0, 7};

    QxArray a = gfx_array(bytes, 2, 1);
    CALL("surface.write", I(s), I(0), I(0), I(2), I(1), ARRAY(&a));

    gfx_draw_vertex p[4];
    band(p, 0, 3, 2, 2);
    CALL("draw.clear_depth");
    CHECK(!poly(S, s, p, 4, 0, GFX_DRAW_ZTEST | GFX_DRAW_ZSET));
    ROW(0, "1177");

    flat(S, 0, 3, 4, 6, GFX_DRAW_ZTEST);
    ROW(0, "6677");
    ROW(1, "6677");
}

/* At one depth, perspective's texels are affine's: a quad turned off the grid, drawn both ways. */
TEST(gfx_poly_level_perspective_is_affine)
{
    const int32_t s_w = 4;
    gfx_draw_vertex p[] = {
        {1.3f, 0.6f, 3, 0, 0},
        {9.2f, 2.1f, 3, 4, 0},
        {7.9f, 9.4f, 3, 4, 4},
        {0.4f, 7.7f, 3, 0, 4}
    };

    int32_t got[2][10][10];

    for (int32_t k = 0; k < 2; k++) {
        CANVAS(10, 10);
        const int32_t s = CALL("surface.new", I(s_w), I(4)).result.i;
        uint8_t bytes[16];

        for (uint32_t i = 0; i < 16; i++)
            bytes[i] = (uint8_t)(2 + i);

        QxArray a = gfx_array(bytes, 16, 1);
        CALL("surface.write", I(s), I(0), I(0), I(4), I(4), ARRAY(&a));
        CHECK(!poly(S, s, p, 4, 0, k ? GFX_DRAW_PERSP : 0));

        for (int32_t y = 0; y < 10; y++)
            for (int32_t x = 0; x < 10; x++)
                got[k][y][x] = PX(x, y);
    }

    int32_t differ = 0, drawn = 0;

    for (int32_t y = 0; y < 10; y++)
        for (int32_t x = 0; x < 10; x++) {
            differ += got[0][y][x] != got[1][y][x];
            drawn += got[0][y][x] != 1;
        }

    CHECK(drawn > 40);
    CHECK(differ == 0);
}

/* A point nearer than 1 counts only with PERSP or a depth flag; more than 32 points never. */
TEST(gfx_poly_refuses)
{
    CANVAS(4, 2);
    gfx_draw_vertex p[33];
    band(p, 0, 3, 0.5f, 1);
    CHECK(!poly(S, 0, p, 4, 3, GFX_DRAW_FILL));
    CHECK(poly(S, 0, p, 4, 3, GFX_DRAW_FILL | GFX_DRAW_ZSET) == 5);

    for (uint32_t i = 0; i < 33; i++)
        p[i] = (gfx_draw_vertex){(float)(i % 4), (float)(i / 4 % 2), 1, 0, 0};

    CHECK(poly(S, 0, p, 33, 3, GFX_DRAW_FILL) == 5);
}
