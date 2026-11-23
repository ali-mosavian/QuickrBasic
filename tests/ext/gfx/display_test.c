/* gfx.display: the canvas opened at the depth asked, the frame's commands as drawn, the flip. */
#include "test.h"

TEST(gfx_open_gives_the_depth_asked)
{
    gfx_fresh(S);
    CHECK(CALL("display.open", I(320), I(200), I(32), I(GFX_DISPLAY_VSYNC)).error == 0);
    CHECK(CALL("surface.bits", I(GFX_SURFACE_CANVAS)).result.i == 32);
    CHECK(CALL("surface.width", I(GFX_SURFACE_CANVAS)).result.i == 320);
    CHECK(CALL("display.open", I(320), I(200), I(24), I(0)).error == 5);
    CHECK(CALL("display.open", I(0), I(200), I(8), I(0)).error == 5);
    CHECK(CALL("display.count").error == 73);
}

/* Each call is logged after the camera, with the pen's numbers, until the flip. */
TEST(gfx_commands_drawn)
{
    gfx_fresh(S);
    CALL("display.open", I(16), I(16), I(8), I(0));
    const int32_t s = CALL("surface.new", I(2), I(2)).result.i;
    CALL("draw.camera", F(10), F(-2));
    CALL("draw.rect", F(10), F(0), F(12), F(1), I(4), I(GFX_DRAW_FILL));
    CALL("draw.sprite", I(s), F(11), F(3), I(GFX_DRAW_FLIPY));
    CHECK(CALL("display.draws").result.i == 2);

    gfx_display_command d;
    CHECK(!CALL("display.drawn", I(1), REF(&d)).error);
    CHECK(d.kind == GFX_DISPLAY_DREW_RECT && d.target == GFX_SURFACE_CANVAS && d.colour == 4);
    CHECK(d.x == 0 && d.y == 2 && d.x2 == 2 && d.y2 == 3 && d.flags == GFX_DRAW_FILL);
    CHECK(d.light == 63 && d.alpha == 63 && d.tint == 0xFFFFFF && d.scale == 1);
    CHECK(!CALL("display.drawn", I(2), REF(&d)).error);
    CHECK(d.kind == GFX_DISPLAY_DREW_SPRITE && d.source == s && d.x == 1 && d.y == 5);
    CHECK(PX(0, 2) == 4 && PX(2, 3) == 4 && PX(3, 3) == 0);

    CHECK(CALL("display.drawn", I(3), REF(&d)).error == 9);
    CALL("display.flip");
    CHECK(CALL("display.draws").result.i == 0);
    CHECK(PX(0, 2) == 4); /* the canvas read after the flip is the frame shown */
}

TEST(gfx_closes_with_the_run)
{
    const QxModule *const m = gfx_fresh(S);
    CALL("display.open", I(4), I(4), I(8), I(0));
    CALL("display.flip");
    PgfxFrame f;
    CHECK(pgfx_shown(&f) && f.w == 4);
    m->stop();
    CHECK(!pgfx_shown(&f));
}
