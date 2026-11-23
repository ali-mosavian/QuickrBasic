/* gfx.draw at 8 bits in software, by its pixels read: the top and left edges, clipping, the key,
   flips, anchors, tiles, text; and the flags a call refuses. */
#include "test.h"

/* An 8 by 4 canvas cleared to 1. */
static void canvas(Session *const S) { CANVAS(8, 4); }

/* Corners name pixel centres: a rect's edges lie half a pixel out, and a pixel whose centre is on
   the top or left edge is drawn, on the bottom or right one not. */
TEST(gfx_rect_top_and_left_edges)
{
    canvas(S);
    CALL("draw.rect", F(1), F(1), F(3), F(2), I(5), I(GFX_DRAW_FILL));
    ROW(0, "11111111");
    ROW(1, "15551111");
    ROW(2, "15551111");
    CALL("draw.rect", F(4.5), F(0.5), F(6.5), F(1.5), I(7), I(GFX_DRAW_FILL));
    ROW(0, "11117771"); /* edges at x 4 and 7, y 0 and 2 */
    ROW(1, "15557771");
    ROW(2, "15551111");
    ROW(3, "11111111");
}

TEST(gfx_rect_outline_and_line)
{
    canvas(S);
    CALL("draw.rect", F(0), F(0), F(3), F(3), I(2), I(0));
    ROW(0, "22221111");
    ROW(1, "21121111");
    ROW(3, "22221111");
    CALL("draw.line", F(4), F(0), F(7), F(3), I(3));
    ROW(0, "22223111");
    ROW(3, "22221113");
    CALL("draw.line", F(-1e9), F(2), F(1e9), F(2), I(4)); /* far ends: cut to the clip */
    ROW(2, "44444444");
}

TEST(gfx_clip_bounds_every_call)
{
    canvas(S);
    CALL("pen.clip", I(2), I(1), I(3), I(2));
    CALL("draw.clear", I(6));
    CALL("draw.line", F(0), F(3), F(7), F(0), I(8));
    ROW(0, "11111111");
    ROW(1, "11668111"); /* the line in the clip: (2, 2), (3, 2), (4, 1) */
    ROW(2, "11886111");
    ROW(3, "11111111");
    CALL("pen.unclip");
    CALL("draw.pset", F(0), F(0), I(9));
    ROW(0, "91111111");
}

/* A 3 by 2 sheet: 0 7 9 above 2 0 3. */
static int32_t sheet(Session *const S)
{
    const int32_t s = CALL("surface.new", I(3), I(2)).result.i;
    uint8_t bytes[] = {0, 7, 9, 2, 0, 3};

    QxArray a = gfx_array(bytes, 6, 1);
    CHECK(!CALL("surface.write", I(s), I(0), I(0), I(3), I(2), ARRAY(&a)).error);
    return s;
}

TEST(gfx_sprite_key_flips_and_opaque)
{
    canvas(S);
    const int32_t s = sheet(S);
    CALL("draw.sprite", I(s), F(0), F(0), I(0));
    CALL("draw.sprite", I(s), F(4), F(0), I(GFX_DRAW_FLIPX | GFX_DRAW_FLIPY));
    ROW(0, "17913121");
    ROW(1, "21319711");
    CALL("draw.sprite", I(s), F(0), F(2), I(GFX_PEN_OPAQUE | GFX_DRAW_FLIPX));
    ROW(2, "97011111");
    ROW(3, "30211111");
}

/* The anchor is the texel a sprite's x, y names; a sprite half off the canvas is clipped. */
TEST(gfx_sprite_anchor_and_edges)
{
    canvas(S);
    const int32_t s = sheet(S);
    CALL("surface.anchor", I(s), I(2), I(1));
    CALL("draw.sprite", I(s), F(2), F(1), I(0));
    ROW(0, "17911111");
    CALL("draw.sprite", I(s), F(0), F(3), I(GFX_PEN_OPAQUE));
    ROW(2, "91111111"); /* texel 2, 0 at pixel 0, 2: texels 0 and 1 off the left */
    ROW(3, "31111111");
    CALL("draw.sprite", I(s), F(8.4), F(1), I(GFX_PEN_OPAQUE)); /* its top left at 6.4: pixel 6's */
    ROW(0, "17911107");
    ROW(1, "21311120");
}

TEST(gfx_tiles_and_text)
{
    canvas(S);
    const int32_t s = sheet(S);
    int32_t views[6];
    QxArray v = gfx_array(views, 6, 4);
    CHECK(CALL("surface.grid", I(s), I(1), I(1), ARRAY(&v)).result.i == 6);
    int32_t cells[] = {views[1], 0, views[2], views[5]};

    QxArray map = gfx_array(cells, 4, 4);
    CHECK(!CALL("draw.tiles", ARRAY(&map), I(2), F(5), F(2), I(2), I(1), I(0)).error);
    ROW(2, "11111711");
    ROW(3, "11111913");

    CHECK(!CALL("draw.text", F(0), F(0), STR("\xdb!"), I(4)).error); /* a full cell, then a ! */
    ROW(0, "44444444");
    CHECK(CALL("draw.text_width", STR("abc")).result.i == 24);

    cells[0] = 0x10000077; /* a surface's kind, never made */
    CHECK(CALL("draw.tiles", ARRAY(&map), I(2), F(0), F(0), I(1), I(1), I(0)).error == 5);
}

/* Each call takes its own flags: another's is illegal, one drawn later not yet available. */
TEST(gfx_flags_a_call_refuses)
{
    canvas(S);
    const int32_t s = sheet(S);
    CHECK(CALL("draw.sprite", I(s), F(0), F(0), I(GFX_DRAW_FILL)).error == 5);
    CHECK(CALL("draw.rect", F(0), F(0), F(1), F(1), I(1), I(GFX_PEN_BLEND)).error == 73);
    CHECK(CALL("draw.rect", F(0), F(0), F(1), F(1), I(256), I(0)).error == 5);
    CHECK(PX(0, 0) == 1);
}
