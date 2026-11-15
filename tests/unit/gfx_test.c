/* The graphics statements run: what they draw on the runtime's screen (csrc/core/comp/gfxstmt.c,
   csrc/core/vm/gfx.c). */
#include "session.h"
#include "unit.h"

/* The headless device's framebuffer (llrm's dev/host/video.h). */
unsigned host_width(void);
const uint8_t *host_pixels(void);

static uint8_t pixel(const uint32_t x, const uint32_t y)
{
    return host_pixels()[y * host_width() + x];
}

static Status draw(Session *const S, const char *const line)
{
    const char *const lines[] = {"10 SCREEN 13", line, nullptr};

    return unit_run(S, lines);
}

TEST(pset_and_preset)
{
    CHECK(draw(S, "20 PSET (10, 20), 4: PSET STEP (1, 1), 5: PRESET (3, 3)") == ST_OK);
    CHECK(pixel(10, 20) == 4 && pixel(11, 21) == 5 && pixel(3, 3) == 0);
}

TEST(line_box_and_fill)
{
    CHECK(
        draw(
            S,
            "20 LINE (0, 0)-(9, 0), 2: LINE (20, 20)-(29, 29), 3, B: LINE (40, 40)-(49, 49), 6, BF"
        ) == ST_OK
    );
    CHECK(pixel(0, 0) == 2 && pixel(9, 0) == 2);
    CHECK(pixel(20, 25) == 3 && pixel(25, 25) == 0); /* B: the edge, not the inside */
    CHECK(pixel(45, 45) == 6);                       /* BF: filled */
}

TEST(line_from_the_last_point)
{
    CHECK(draw(S, "20 PSET (5, 5), 1: LINE -(5, 15), 7") == ST_OK);
    CHECK(pixel(5, 10) == 7);
}

TEST(circle)
{
    CHECK(draw(S, "20 CIRCLE (100, 100), 10, 9") == ST_OK);
    CHECK(pixel(110, 100) == 9 && pixel(100, 100) == 0);
}

/* Not a mode the screen has: Illegal function call, from the runtime. */
TEST(screen_bad_mode)
{
    static const char *const lines[] = {"10 SCREEN 5", nullptr};

    CHECK(unit_run(S, lines) == E_USR);
}

TEST(paint_fills_a_box)
{
    CHECK(draw(S, "20 LINE (0, 0)-(20, 20), 3, B: PAINT (10, 10), 5, 3") == ST_OK);
    CHECK(pixel(10, 10) == 5 && pixel(0, 10) == 3 && pixel(25, 25) == 0);
}

TEST(draw_moves_and_lines)
{
    CHECK(draw(S, "20 PSET (50, 50), 0: DRAW \"C2 R10 D10\"") == ST_OK);
    CHECK(pixel(55, 50) == 2 && pixel(60, 55) == 2);
}

TEST(view_moves_the_origin)
{
    CHECK(draw(S, "20 VIEW (100, 100)-(150, 150): PSET (1, 1), 4: VIEW: PSET (1, 1), 6") == ST_OK);
    CHECK(pixel(101, 101) == 4 && pixel(1, 1) == 6);
}

TEST(window_maps_points)
{
    CHECK(draw(S, "20 WINDOW SCREEN (0, 0)-(31.9, 19.9): PSET (1, 1), 7") == ST_OK);
    CHECK(pixel(10, 10) == 7);
}

TEST(point_and_pmap)
{
    static const char *const lines[] = {
        "10 SCREEN 13: PSET (3, 4), 9",
        "20 PRINT POINT(3, 4); POINT(400, 4)",
        "30 WINDOW (0, 0)-(319, 199): PRINT PMAP(0, 1)",
        nullptr,
    };

    CHECK(unit_run(S, lines) == ST_OK);
    CHECK_TEXT(S->cons.sbuf, S->cons.sblen, " 9 -1 \r 199 \r");
}

TEST(get_and_put)
{
    CHECK(
        draw(S, "20 DIM a(100): PSET (1, 1), 4: GET (0, 0)-(3, 3), a: PUT (10, 10), a, PSET") ==
        ST_OK
    );
    CHECK(pixel(11, 11) == 4 && pixel(10, 10) == 0);
}

TEST(palette_needs_graphics)
{
    static const char *const lines[] = {"10 SCREEN 0: PALETTE 1, 2", nullptr};

    CHECK(unit_run(S, lines) == E_USR);
    CHECK(draw(S, "20 PALETTE 1, 2") == ST_OK);
}
