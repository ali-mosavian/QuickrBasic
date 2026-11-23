/* gfx.draw's circles, arcs and polygons at 8 bits, and the turned camera, by their pixels read.
   Each expectation is worked by hand from the rules: a pixel is drawn when its centre is inside,
   or on the top or left edge. */
#include "gmath.h"
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
    gfx_draw_vec2 tri[] = {{9, 9}, {1, 1}, {6, 1}, {1, 6}};

    QxArray a = gfx_array(tri, 4, sizeof *tri);
    a.lbound = -1; /* points(-1 to 2): the triangle from index 0 */

    CANVAS(8, 8);
    CHECK(!CALL("draw.poly", ARRAY(&a), I(0), I(3), I(3), I(GFX_DRAW_FILL)).error);
    ROW(0, "11111111");
    ROW(1, "13333311");
    ROW(2, "13333111");
    ROW(3, "13331111");
    ROW(4, "13311111");
    ROW(5, "13111111");
    ROW(6, "11111111");

    /* each edge without its end, the next one's start: each corner once */
    CANVAS(8, 8);
    CALL("draw.poly", ARRAY(&a), I(0), I(3), I(4), I(0));
    ROW(1, "14444441");
    ROW(2, "14111411");
    ROW(3, "14114111");
    ROW(4, "14141111");
    ROW(5, "14411111");
    ROW(6, "14111111");

    CHECK(CALL("draw.poly", ARRAY(&a), I(0), I(2), I(4), I(0)).error == 5);
    CHECK(CALL("draw.poly", ARRAY(&a), I(1), I(3), I(4), I(0)).error == 9);
    CHECK(CALL("draw.poly", ARRAY(&a), I(-2), I(3), I(4), I(0)).error == 9);
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
