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
