/* gfx.surface: views over their parents' pixels, grids, aim, stale handles, a BMP loaded. */
#include "test.h"
#include <stdio.h>

TEST(gfx_views_draw_into_their_parents)
{
    gfx_fresh(S);
    CHECK(!CALL("display.open", I(8), I(4), I(8), I(0)).error);
    const int32_t v = CALL("surface.view", I(GFX_SURFACE_CANVAS), I(2), I(1), I(3), I(2)).result.i;
    CHECK(v && CALL("surface.width", I(v)).result.i == 3);
    CALL("pen.target", I(v));
    CALL("draw.clear", I(5));
    CALL("draw.pset", F(0), F(0), I(6)); /* the view's own top left */
    CHECK(PX(2, 1) == 6 && PX(3, 1) == 5 && PX(4, 2) == 5);
    CHECK(PX(5, 1) == 0 && PX(1, 1) == 0 && PX(2, 3) == 0); /* clipped to the view */

    CALL("surface.aim", I(v), I(0), I(0));
    CALL("draw.pset", F(0), F(0), I(7));
    CHECK(PX(0, 0) == 7);
    CHECK(CALL("surface.aim", I(v), I(6), I(0)).error == 5); /* out of its parent */
    CHECK(CALL("surface.view", I(GFX_SURFACE_CANVAS), I(6), I(0), I(3), I(1)).error == 5);
}

TEST(gfx_grid_fills_views_row_by_row)
{
    gfx_fresh(S);
    const int32_t s = CALL("surface.new", I(5), I(4)).result.i;
    int32_t views[4] = {};

    QxArray a = gfx_array(views, 3, 4);
    CHECK(CALL("surface.grid", I(s), I(2), I(2), ARRAY(&a)).error == 9); /* 4 cells, 3 places */
    a.count = 4;
    CHECK(CALL("surface.grid", I(s), I(2), I(2), ARRAY(&a)).result.i == 4);

    uint8_t bytes[] = {9};

    QxArray b = gfx_array(bytes, 1, 1);
    CALL("surface.write", I(views[3]), I(1), I(1), I(1), I(1), ARRAY(&b));
    CHECK(CALL("surface.point", I(s), I(3), I(3)).result.i == 9);
}

/* A freed surface's handle, its views' and a handle of no surface are errors, never pixels. */
TEST(gfx_stale_handles)
{
    gfx_fresh(S);
    const int32_t s = CALL("surface.new", I(4), I(4)).result.i;
    const int32_t v = CALL("surface.view", I(s), I(0), I(0), I(2), I(2)).result.i;
    const int32_t w = CALL("surface.view", I(v), I(1), I(1), I(1), I(1)).result.i;
    CHECK(!CALL("surface.free", I(s)).error);
    CHECK(CALL("surface.width", I(s)).error == 5);
    CHECK(CALL("surface.width", I(v)).error == 5);
    CHECK(CALL("surface.width", I(w)).error == 5);

    const int32_t again = CALL("surface.new", I(1), I(1)).result.i; /* s's slot again */
    CHECK(again && (again & 0xFFFFF) == (s & 0xFFFFF) && again != s);
    CHECK(CALL("surface.width", I(s)).error == 5);
    CHECK(CALL("surface.width", I(0)).error == 5);
    CHECK(CALL("surface.width", I(12345)).error == 5);

    CHECK(CALL("draw.clear", I(1)).error == 5); /* the canvas before open */
    CHECK(!CALL("display.open", I(2), I(2), I(8), I(0)).error);
    CHECK(CALL("surface.free", I(GFX_SURFACE_CANVAS)).error == 5);
    CALL("display.close");
    CHECK(CALL("draw.clear", I(1)).error == 5);
}

/* A BMP of 3 by 2 at 8 bits, as a paint program writes one: its rows bottom first, each padded to
   4 bytes; its colours black, red and green, B G R and a 0 each. */
static uint32_t bmp(uint8_t *const f)
{
    static const uint8_t colours[] = {0, 0, 0, 0, 0, 0, 255, 0, 0, 255, 0, 0};

    static const uint8_t rows[] = {2, 2, 1, 0, 1, 0, 2, 0}; /* the bottom row, then the top */

    memset(f, 0, 74);
    memcpy(f, "BM", 2);
    store32(f + 2, 74);
    store32(f + 10, 66); /* the pixels' offset */
    store32(f + 14, 40); /* the info header's bytes */
    store32(f + 18, 3);
    store32(f + 22, 2);
    store16(f + 26, 1); /* planes */
    store16(f + 28, 8); /* bits a pixel */
    store32(f + 46, 3); /* colours */
    memcpy(f + 54, colours, sizeof colours);
    memcpy(f + 66, rows, sizeof rows);
    return 74;
}

TEST(gfx_load_a_bmp)
{
    gfx_fresh(S);
    const char path[] = "build/host/gfx/test.bmp";
    uint8_t bytes[74];
    FILE *const f = fopen(path, "wb");
    CHECK(f && fwrite(bytes, bmp(bytes), 1, f) == 1);
    fclose(f);

    const Called got = CALL("surface.load", STR("build/host/gfx/test.bmp"));
    const int32_t s = got.result.i;
    CHECK(
        !got.error && CALL("surface.width", I(s)).result.i == 3 &&
        CALL("surface.height", I(s)).result.i == 2
    );
    CHECK(CALL("surface.point", I(s), I(0), I(0)).result.i == 1); /* the top row first */
    CHECK(CALL("surface.point", I(s), I(2), I(1)).result.i == 1);
    CHECK(!CALL("display.palette_of", I(0), I(s)).error);
    CHECK(CALL("display.rgb", I(250), I(0), I(0)).result.i == 1);
    CHECK(CALL("display.rgb", I(0), I(200), I(9)).result.i == 2);

    CHECK(CALL("surface.load", STR("build/host/gfx/none.bmp")).error == 53);
    CHECK(CALL("surface.load", STR("tests/ext/gfx/call.h")).error == 5);
    remove(path);
}
