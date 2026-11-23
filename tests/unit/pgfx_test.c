/* The host's minor 1 (io/native.c, plat/host/pgfx.c): gfx's screen lent as memory, the frame shown
   a copy, the screen closed with the run; the clock; a file read whole. */
#include "plat/pgfx.h"
#include "session.h"
#include "unit.h"

TEST(pgfx_lends_a_page_and_shows_a_copy)
{
    const QxHost *const h = &S->natv.host;
    QxScreen s = {.w = 4, .h = 2, .depth = 8};

    CHECK(h->minor >= 1 && h->gfx_open(&s));
    CHECK(!s.gpu && s.out_w == 4 && s.out_h == 2);

    uint32_t pitch;
    uint8_t *const page = h->gfx_page(&pitch);
    PgfxFrame f;
    CHECK(page && pitch == 4 && !page[7]);
    CHECK(!pgfx_shown(&f)); /* nothing shown before the first */

    page[7] = 9;
    uint32_t palette[256] = {[9] = 0x123456};

    h->gfx_show(palette);
    page[7] = 1; /* the next frame: the shown one keeps its pixels */
    CHECK(pgfx_shown(&f) && f.w == 4 && f.h == 2);
    CHECK(f.pixels[7] == 9 && f.palette[9] == 0x123456);
    CHECK(((uint8_t *)h->gfx_page(&pitch))[7] == 1);

    h->gfx_close();
    CHECK(!pgfx_shown(&f) && !h->gfx_page(&pitch));
    s.depth = 16;
    CHECK(!h->gfx_open(&s));
}

/* A screen left open by an extension is the runtime's again once the run ends. */
TEST(pgfx_closes_with_the_run)
{
    QxScreen s = {.w = 2, .h = 2, .depth = 8};

    CHECK(S->natv.host.gfx_open(&s));
    S->natv.host.gfx_show((const uint32_t[256]){});

    natv_start(&S->natv);
    natv_stop(&S->natv);
    PgfxFrame f;
    CHECK(!pgfx_shown(&f));
}

TEST(pgfx_clock_and_file)
{
    const QxHost *const h = &S->natv.host;
    const uint64_t t = h->clock_us();
    CHECK(h->clock_us() >= t);

    const char path[] = "tests/unit/pgfx_test.c";
    uint32_t n = 0;
    uint8_t *const text = h->file_read(path, sizeof path - 1, &n);
    CHECK(text && n > 100 && !memcmp(text, "/* The host's minor 1", 21));
    h->free(text);
    CHECK(!h->file_read("nosuch.bmp", 10, &n));
}
