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
    return CALL("draw.poly", I(SURF_SCREEN), ARRAY(&a), I(0), I(n), I(s), I(colour), I(flags))
        .error;
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
    enum { BOTH = GFX_DRAW_DEPTH_TEST | GFX_DRAW_DEPTH_WRITE };

    CANVAS(8, 2);
    CALL("surface.clear_depth", I(SURF_SCREEN));
    CHECK(!flat(S, 0, 3, 2, 5, BOTH));
    flat(S, 0, 5, 4, 6, BOTH);
    ROW(0, "55556611");
    ROW(1, "55556611");

    flat(S, 2, 7, 2, 7, GFX_DRAW_DEPTH_TEST);
    ROW(0, "55557777");

    flat(S, 4, 7, 3, 3, GFX_DRAW_DEPTH_TEST);
    ROW(1, "55553333");

    flat(S, 0, 1, 8, 2, GFX_DRAW_DEPTH_WRITE);
    flat(S, 0, 3, 4, 4, GFX_DRAW_DEPTH_TEST);
    ROW(0, "44553333");

    CALL("surface.clear_depth", I(SURF_SCREEN));
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
    CALL("surface.clear_depth", I(SURF_SCREEN));
    CHECK(!poly(S, s, p, 4, 0, GFX_DRAW_DEPTH_TEST | GFX_DRAW_DEPTH_WRITE));
    ROW(0, "1177");

    flat(S, 0, 3, 4, 6, GFX_DRAW_DEPTH_TEST);
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
        CHECK(!poly(S, s, p, 4, 0, k ? GFX_DRAW_PERSPECTIVE : 0));

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
    CHECK(poly(S, 0, p, 4, 3, GFX_DRAW_FILL | GFX_DRAW_DEPTH_WRITE) == 5);

    for (uint32_t i = 0; i < 33; i++)
        p[i] = (gfx_draw_vertex){(float)(i % 4), (float)(i / 4 % 2), 1, 0, 0};

    CHECK(poly(S, 0, p, 33, 3, GFX_DRAW_FILL) == 5);
}

static uint32_t next(uint32_t *const seed)
{
    *seed = *seed * 1103515245u + 12345u;
    return *seed >> 8;
}

/* lo to hi, at random */
static float between(uint32_t *const seed, const float lo, const float hi)
{
    return lo + (hi - lo) * (float)(next(seed) & 0xFFFF) / 65535.0f;
}

/* The scanner clips a polygon once and its fillers test no bound, so a row must come out inside
   the clip: random polygons over its edges, flat, affine, perspective and deep, a texture held
   and one tiled, some with a corner not finite or 1e30 off, change no pixel outside. Without the
   clip, rows ran past the screen's memory and the run died. */
TEST(gfx_poly_rows_stay_in_the_clip)
{
    enum { W = 32, H = 24, CX = 8, CY = 6, CW = 12, CH = 9 };

    CANVAS(W, H);
    CALL("surface.set_clip", I(SURF_SCREEN), I(CX), I(CY), I(CW), I(CH));
    CALL("surface.clear_depth", I(SURF_SCREEN));

    const int32_t held = CALL("surface.new", I(5), I(3)).result.i;
    const int32_t tiled = CALL("surface.new", I(8), I(8)).result.i;
    uint8_t bytes[64];

    for (uint32_t i = 0; i < 64; i++)
        bytes[i] = (uint8_t)(2 + i % 8);

    QxArray a = gfx_array(bytes, 64, 1);
    CALL("surface.write", I(held), I(0), I(0), I(5), I(3), ARRAY(&a));
    CALL("surface.write", I(tiled), I(0), I(0), I(8), I(8), ARRAY(&a));

    const float near[] = {-1e-3f, 0, 1e-3f, 0.5f};
    const int32_t flags[] = {
        GFX_DRAW_FILL,
        0,
        GFX_DRAW_PERSPECTIVE,
        GFX_DRAW_DEPTH_TEST | GFX_DRAW_DEPTH_WRITE,
        GFX_DRAW_PERSPECTIVE | GFX_DRAW_DEPTH_TEST | GFX_DRAW_DEPTH_WRITE,
    };
    uint32_t seed = 7;

    for (uint32_t k = 0; k < 3000; k++) {
        gfx_draw_vertex p[8];
        uint32_t n;

        if (k & 1) {
            /* a convex ring about a centre anywhere over the clip */
            const float cx = between(&seed, -8, W + 8), cy = between(&seed, -8, H + 8);
            const float rx = between(&seed, 0.5f, 24), ry = between(&seed, 0.5f, 24);
            const float turn = between(&seed, 0, 6.2831853f);
            n = 3 + next(&seed) % 6;

            for (uint32_t i = 0; i < n; i++) {
                const float t = turn + 6.2831853f * (float)i / (float)n;
                p[i] = (gfx_draw_vertex){cx + rx * __builtin_cosf(t), cy + ry * __builtin_sinf(t),
                                         between(&seed, 1, 8), between(&seed, -8, 16),
                                         between(&seed, -8, 16)};
            }
        } else {
            /* a box whose sides lie on or about the clip's: its edges at half pixels */
            const float l = CX - 0.5f + near[next(&seed) % 4] - (float)(next(&seed) % 3);
            const float r = CX + CW - 0.5f - near[next(&seed) % 4] + (float)(next(&seed) % 3);
            const float t = CY - 0.5f + near[next(&seed) % 4] - (float)(next(&seed) % 3);
            const float b = CY + CH - 0.5f - near[next(&seed) % 4] + (float)(next(&seed) % 3);
            n = 4;
            p[0] = (gfx_draw_vertex){l, t, 2, 0, 0};
            p[1] = (gfx_draw_vertex){r, t, 2, 5, 0};
            p[2] = (gfx_draw_vertex){r, b, 2, 5, 3};
            p[3] = (gfx_draw_vertex){l, b, 2, 0, 3};
        }

        /* now and then a corner not finite or far off, which the clip must not pass */
        const float wild[] = {__builtin_nanf(""), __builtin_inff(), -__builtin_inff(), 1e30f, -1e30f};

        if (k % 7 == 0) {
            const float w = wild[next(&seed) % 5];
            gfx_draw_vertex *const at = &p[next(&seed) % n];

            if (next(&seed) & 1)
                at->x = w;
            else
                at->y = w;
        }

        const int32_t f = flags[next(&seed) % 5];
        const int32_t s = f & GFX_DRAW_FILL ? 0 : next(&seed) & 1 ? tiled : held;
        poly(S, s, p, (int32_t)n, (int32_t)(2 + k % 8), f);
    }

    int32_t outside = 0;

    for (int32_t y = 0; y < H; y++)
        for (int32_t x = 0; x < W; x++)
            if (x < CX || x >= CX + CW || y < CY || y >= CY + CH)
                outside += PX(x, y) != 1;

    CHECK(outside == 0);
}
