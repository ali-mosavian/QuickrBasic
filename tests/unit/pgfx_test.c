/* The host's minor 1 (io/native.c, plat/host/pgfx.c): gfx's screen lent as memory, the frame shown
   a copy, the screen closed with the run; the clock; a file read whole. */
#include <pthread.h>
#include <time.h>

#include "plat/host/host.h"
#include "plat/pgfx.h"
#include "qx/qxlist.h"
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
    s.depth = 24;
    CHECK(!h->gfx_open(&s));
}

/* 16 bits: two bytes a pixel, the page's pitch and the frame shown its depth's. */
TEST(pgfx_lends_pages_of_each_depth)
{
    const QxHost *const h = &S->natv.host;
    QxScreen s = {.w = 3, .h = 2, .depth = 16};

    uint32_t pitch;
    PgfxFrame f;

    CHECK(h->gfx_open(&s));
    uint8_t *const page = h->gfx_page(&pitch);
    CHECK(pitch == 6);
    page[11] = 0xF8;
    h->gfx_show((const uint32_t[256]){});

    CHECK(pgfx_shown(&f) && f.depth == 16 && f.pixels[11] == 0xF8);
    h->gfx_close();

    s.depth = 32;
    CHECK(h->gfx_open(&s) && h->gfx_page(&pitch) && pitch == 12);
    h->gfx_close();
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

/* A display's frame, shown 20 ms from now on another thread. */
static void *display_frame(void *const arg)
{
    (void)arg;
    nanosleep(&(const struct timespec){.tv_nsec = 20000000}, nullptr);

    host_frame_shown();
    return nullptr;
}

static const uint32_t SHOWS[] = {QX_LIST_SHOW | 2 << QX_LIST_SIZE, 1, QX_LIST_END};

/* The frames shown once a page, or a list, is shown, a display counting them and showing one 20 ms
   on. */
static uint32_t shown_after(const QxHost *const h, const bool list)
{
    host_display()->frames = 1;
    pthread_t t;

    if (pthread_create(&t, nullptr, display_frame, nullptr))
        return 0;

    if (list)
        h->gpu_play(SHOWS, sizeof SHOWS / sizeof SHOWS[0]);
    else
        h->gfx_show((const uint32_t[256]){});

    const uint32_t n = host_display()->frames;
    pthread_join(t, nullptr);
    host_display()->frames = 0; /* no display again, for the tests after */
    return n;
}

static bool fake_open(const uint16_t w, const uint16_t h) { return (void)w, (void)h, true; }

static void fake_close(void) {}

static bool fake_play(const uint32_t *const list, const uint32_t words)
{
    return words && (list[0] & 0xFF) == QX_LIST_SHOW;
}

static const PgfxPlayer fake = {
    .shaders = 1 << QX_SHADER_MSL,
    .open = fake_open,
    .close = fake_close,
    .play = fake_play,
};

/* VSYNC: a frame shown, page or list, waits for the display to show one; without, it does not. */
TEST(pgfx_vsync_waits_for_the_display)
{
    const QxHost *const h = &S->natv.host;
    QxScreen s = {
        .w = 2,
        .h = 2,
        .depth = 8,
        .flags = QX_SCREEN_VSYNC,
    };

    CHECK(h->gfx_open(&s) && !s.gpu);
    CHECK(shown_after(h, false) == 2);
    s.flags = 0;
    CHECK(h->gfx_open(&s));
    CHECK(shown_after(h, false) == 1);

    pgfx_set_player(&fake);
    s = (QxScreen){
        .w = 2,
        .h = 2,
        .depth = 32,
        .flags = QX_SCREEN_VSYNC,
        .shaders = 1 << QX_SHADER_MSL,
    };

    CHECK(h->gfx_open(&s) && s.gpu);
    CHECK(shown_after(h, true) == 2);
    s.flags = 0;
    CHECK(h->gfx_open(&s));
    CHECK(shown_after(h, true) == 1);
    h->gfx_close();
    pgfx_set_player(nullptr);
}

/* The refresh the display gives, in Hz. */
TEST(pgfx_gives_the_refresh)
{
    QxScreen s = {.w = 2, .h = 2, .depth = 8};

    host_display()->millihz = 59940;
    CHECK(S->natv.host.gfx_open(&s));
    host_display()->millihz = 0;
    CHECK(s.hz > 59.93F && s.hz < 59.95F);
    CHECK(S->natv.host.gfx_open(&s) && !s.hz);
    S->natv.host.gfx_close();
}

/* What the display saw since the screen opened waits for the program; a closed window's QUIT only
   once it has asked, else the window ends the run. */
TEST(pgfx_events_wait_for_the_program)
{
    const QxHost *const h = &S->natv.host;
    QxScreen s = {.w = 2, .h = 2, .depth = 8};

    QxEvent e;

    CHECK(host_event_push(&(QxEvent){.kind = QX_EVENT_FOCUS}));

    CHECK(h->gfx_open(&s) && !h->gfx_event(&e)); /* before it opened */

    h->gfx_close();
    CHECK(h->gfx_open(&s));
    CHECK(!host_event_push(&(QxEvent){.kind = QX_EVENT_QUIT}));

    CHECK(!h->gfx_event(&e));
    CHECK(host_event_push(&(QxEvent){.kind = QX_EVENT_QUIT}));

    CHECK(h->gfx_event(&e) && e.kind == QX_EVENT_QUIT);

    for (uint32_t i = 0; i < HOST_EVENTS; i++)
        CHECK(host_event_push(&(QxEvent){.kind = QX_EVENT_MOVED, .monitor = (uint8_t)i}));

    CHECK(!host_event_push(&(QxEvent){.kind = QX_EVENT_MOVED}));

    CHECK(h->gfx_event(&e) && e.kind == QX_EVENT_MOVED && !e.monitor);
    h->gfx_close();
    CHECK(!host_event_push(&(QxEvent){.kind = QX_EVENT_QUIT})); /* closed: no longer asked */
    CHECK(!h->gfx_monitors() && !h->gfx_mode(1, 0));            /* no display */
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
