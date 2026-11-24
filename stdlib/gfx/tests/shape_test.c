/* gfx.draw's circles, arcs and polygons at 8 bits, and the turned camera, by their pixels read.
   Each expectation is worked by hand from the rules: a pixel is drawn when its centre is inside,
   or on the top or left edge. */
#include "base/disc.h"
#include "base/gmath.h"
#include "test.h"

/* Radius 2 about (3, 3): filled, the centres within 2.5; the outline, from 1.5. Radius 0 is one
   pixel. */
TEST(gfx_circle_filled_and_outlined)
{
    CANVAS(8, 8);
    CALL("draw.circle", F(3), F(3), F(2), I(5), I(GFX_DRAW_FILL));
    ROW(0, "11111111");
    ROW(1, "11555111");
    ROW(2, "15555511");
    ROW(3, "15555511");
    ROW(4, "15555511");
    ROW(5, "11555111");
    ROW(6, "11111111");

    CANVAS(8, 8);
    CALL("draw.circle", F(3), F(3), F(2), I(5), I(0));
    CALL("draw.circle", F(6), F(6), F(0), I(7), I(0));
    ROW(1, "11555111");
    ROW(2, "15111511");
    ROW(3, "15111511");
    ROW(4, "15111511");
    ROW(5, "11555111");
    ROW(6, "11111171");
    CHECK(CALL("draw.circle", F(0), F(0), F(-1), I(1), I(0)).error == 5);
}

/* Clockwise on the screen from start to finish: 0 to a quarter turn is the lower right; past half
   a turn, all but the angles between finish and start (-44.9 to 57.3 degrees here). */
TEST(gfx_arc_sweeps_clockwise)
{
    CANVAS(8, 8);
    CALL("draw.arc", F(3), F(3), F(2), F(0), F(GM_PI / 2), I(5));
    ROW(1, "11111111");
    ROW(2, "11111111");
    ROW(3, "11111511");
    ROW(4, "11111511");
    ROW(5, "11155111");

    CANVAS(8, 8);
    CALL("draw.arc", F(3), F(3), F(2), F(1), F(5.5), I(5));
    ROW(1, "11555111");
    ROW(2, "15111111");
    ROW(3, "15111111");
    ROW(4, "15111111");
    ROW(5, "11555111");
}

TEST(gfx_poly_filled_and_outlined)
{
    gfx_draw_vertex tri[] = {{9, 9, 1, 0, 0}, {1, 1, 1, 0, 0}, {6, 1, 1, 0, 0}, {1, 6, 1, 0, 0}};

    QxArray a = gfx_array(tri, 4, sizeof *tri);
    a.lbound = -1; /* points(-1 to 2): the triangle from index 0 */

    CANVAS(8, 8);
    CHECK(!CALL("draw.poly", I(0), ARRAY(&a), I(0), I(3), I(3), I(GFX_DRAW_FILL)).error);
    /* the centres with x + y < 7, the hypotenuse's own out. qgl's truncated reciprocal once made
       its slope -65535: from row 3 it kept the centre it passes */
    ROW(0, "11111111");
    ROW(1, "13333311");
    ROW(2, "13333111");
    ROW(3, "13331111");
    ROW(4, "13311111");
    ROW(5, "13111111");
    ROW(6, "11111111");

    /* each edge without its end, the next one's start: each corner once */
    CANVAS(8, 8);
    CALL("draw.poly", I(0), ARRAY(&a), I(0), I(3), I(4), I(0));
    ROW(1, "14444441");
    ROW(2, "14111411");
    ROW(3, "14114111");
    ROW(4, "14141111");
    ROW(5, "14411111");
    ROW(6, "14111111");

    CHECK(CALL("draw.poly", I(0), ARRAY(&a), I(0), I(2), I(4), I(0)).error == 5);
    CHECK(CALL("draw.poly", I(0), ARRAY(&a), I(1), I(3), I(4), I(0)).error == 9);
    CHECK(CALL("draw.poly", I(0), ARRAY(&a), I(-2), I(3), I(4), I(0)).error == 9);
}

/* The camera's point at the target's centre (3.5, 3.5), the view a quarter turn clockwise: the
   world's (x, y) lands at (y + 3.3, 3.8 - x). A rect's edges, half a pixel out, turn with it. */
TEST(gfx_camera_turned_turns_and_zooms)
{
    CANVAS(8, 8);
    CHECK(!CALL("draw.camera_turned", F(0.3), F(0.2), F(GM_PI / 2), F(1)).error);
    CALL("draw.rect", F(-2), F(-1), F(2), F(1), I(6), I(GFX_DRAW_FILL));
    ROW(1, "11111111");
    ROW(2, "11666111");
    ROW(4, "11666111");
    ROW(6, "11666111");
    ROW(7, "11111111");

    gfx_display_command c;
    CALL("display.drawn", I(2), REF(&c));
    CHECK(c.kind == GFX_DISPLAY_DREW_RECT && c.scale == 1);
    CHECK(c.x > 2.29f && c.x < 2.31f && c.y > 5.79f && c.y < 5.81f);
    CHECK(c.x2 > 4.29f && c.x2 < 4.31f && c.angle < -1.5707f && c.angle > -1.5709f);

    /* zoomed twice: the rect's edges at 2.5 and 6.5 */
    CANVAS(8, 8);
    CALL("draw.camera_turned", F(0.3), F(0.2), F(0), F(2));
    CALL("draw.rect", F(0.3), F(0.2), F(1.3), F(1.2), I(6), I(GFX_DRAW_FILL));
    ROW(2, "11111111");
    ROW(3, "11166661");
    ROW(6, "11166661");
    ROW(7, "11111111");
    CALL("display.drawn", I(2), REF(&c));
    CHECK(c.scale == 2);
    CHECK(CALL("draw.camera_turned", F(0), F(0), F(0), F(0)).error == 5);
}

/* Each pixel of a 64 by 64 canvas is the colour where disc.h's rule covers it, else the clear. */
static bool follows(Session *const S, const Disc *const k, const Sweep *const w)
{
    for (int32_t y = 0; y < 64; y++)
        for (int32_t x = 0; x < 64; x++) {
            const float dx = (float)x - k->c.x, dy = (float)y - k->c.y;

            if ((PX(x, y) == 5) != (disc_covers(k, dx, dy) && swept(w, dx, dy)))
                return false;
        }

    return true;
}

/* Circles and arcs off the pixel grid cover disc.h's pixels, as a GPU's shader does. The first
   three once drew a pixel outside: the square root's end rounded onto its centre, (25, 22),
   (9, 48) and (17, 33). */
TEST(gfx_circles_follow_the_disc_rule)
{
    static const float edge[][3] =
        {{30, 21.9976349f, 4.5f}, {12.5f, 48.0012474f, 3}, {30.5f, 32.9968262f, 13}};

    const Sweep whole = {.whole = true};

    uint32_t seed = 7;

    for (uint32_t i = 0; i < 3; i++) {
        const Disc k = disc_of((Vec){edge[i][0], edge[i][1]}, edge[i][2], true);

        CANVAS(64, 64);
        CALL("draw.circle", F(k.c.x), F(k.c.y), F(edge[i][2]), I(5), I(GFX_DRAW_FILL));
        CHECK(follows(S, &k, &whole));
    }

    for (uint32_t i = 0; i < 12; i++) {
        float f[5];

        for (uint32_t j = 0; j < 5; j++) {
            seed = seed * 1664525u + 1013904223u;
            f[j] = (float)(seed >> 8) / (1 << 24);
        }

        const Vec c = {16 + f[0] * 32, 16 + f[1] * 32};

        const float r = f[2] * 20, start = f[3] * 7, finish = start + f[4] * 7;
        const Disc k = disc_of(c, r, i % 3 == 0);
        const Sweep w = i % 3 == 2 ? sweep_of(start, finish) : whole;

        CANVAS(64, 64);

        if (i % 3 == 2)
            CALL("draw.arc", F(c.x), F(c.y), F(r), F(start), F(finish), I(5));
        else
            CALL("draw.circle", F(c.x), F(c.y), F(r), I(5), I(i % 3 == 0 ? GFX_DRAW_FILL : 0));

        CHECK(follows(S, &k, &w));
    }
}
