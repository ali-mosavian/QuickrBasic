/* The host's minor 1 (io/native.c, plat/host/pgfx.c): gfx's screen lent as memory, the frame shown
   a copy, the screen closed with the run; the clock; a file read whole. */
#include <pthread.h>
#include <time.h>

#include "plat/host/host.h"
#include "plat/pclk.h"
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

/* The editor at a stop: the screen hidden, its page kept and drawn on, then shown again. */
TEST(pgfx_hides_for_the_editor)
{
    const QxHost *const h = &S->natv.host;
    QxScreen s = {.w = 4, .h = 2, .depth = 8};

    uint32_t pitch, palette[256] = {};

    PgfxFrame f;

    CHECK(!pgfx_hide() && h->gfx_open(&s));
    h->gfx_show(palette);
    CHECK(pgfx_hide() && !pgfx_hide() && !pgfx_shown(&f));

    uint8_t *const page = h->gfx_page(&pitch);
    page[0] = 5;
    h->gfx_show(palette); /* an immediate line's frame */
    CHECK(page && !pgfx_shown(&f));
    CHECK(pgfx_unhide() && !pgfx_unhide() && pgfx_shown(&f) && f.pixels[0] == 5);

    pgfx_hide();
    h->gfx_close();
    CHECK(!pgfx_unhide());
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

enum { LATE_US = 20000, STARVED_US = 150000, HALF_WAIT_US = 50000, TRIES = 20, MEGA = 1000000 };

/* A display's frame, shown late_us on on another thread, and when it showed. */
typedef struct {
    uint64_t late_us;
    uint64_t at;
} Display;

static void *display_frame(void *const arg)
{
    Display *const d = arg;
    nanosleep(
        &(const struct timespec){.tv_sec = d->late_us / MEGA, .tv_nsec = d->late_us % MEGA * 1000},
        nullptr
    );

    d->at = pclk_us();
    host_frame_shown();
    return nullptr;
}

static const uint32_t SHOWS[] = {QX_LIST_SHOW | 2 << QX_LIST_SIZE, 1, QX_LIST_END};

/* The frames shown once a page, or a list, is shown, a display counting them and showing one
   first_us on, then 20 ms on. A run that proves nothing is run again: with vsync, the display
   starved past half the show's 100 ms wait; without, the show back after the display's frame. */
static uint32_t shown_after(
    const QxHost *const h,
    const bool list,
    const bool vsync,
    const uint64_t first_us
)
{
    Display d = {.late_us = first_us};

    for (uint32_t i = 0; i < TRIES; i++, d.late_us = LATE_US) {
        host_display()->frames = 1;
        const uint64_t t0 = pclk_us();
        pthread_t t;

        if (pthread_create(&t, nullptr, display_frame, &d))
            return 0;

        if (list)
            h->gpu_play(SHOWS, sizeof SHOWS / sizeof SHOWS[0]);
        else
            h->gfx_show((const uint32_t[256]){});

        const uint32_t n = host_display()->frames;
        const uint64_t t1 = pclk_us();
        pthread_join(t, nullptr);
        host_display()->frames = 0; /* no display again, for the tests after */
        if (vsync ? d.at - t0 < HALF_WAIT_US : t1 < d.at)
            return n;
    }

    return 0;
}

static bool fake_open(const uint16_t w, const uint16_t h, const uint8_t flags)
{
    return (void)w, (void)h, (void)flags, true;
}

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
    CHECK(shown_after(h, false, true, LATE_US) == 2);
    s.flags = 0;
    CHECK(h->gfx_open(&s));
    CHECK(shown_after(h, false, false, LATE_US) == 1);

    pgfx_set_player(&fake);
    s = (QxScreen){
        .w = 2,
        .h = 2,
        .depth = 32,
        .flags = QX_SCREEN_VSYNC,
        .shaders = 1 << QX_SHADER_MSL,
    };

    CHECK(h->gfx_open(&s) && s.gpu);
    CHECK(shown_after(h, true, true, LATE_US) == 2);
    s.flags = 0;
    CHECK(h->gfx_open(&s));
    CHECK(shown_after(h, true, false, LATE_US) == 1);
    h->gfx_close();
    pgfx_set_player(nullptr);
}

/* VSYNC's check with the display starved past the show's wait once, as on a loaded CI runner: it
   read 1 frame, not 2, and failed. */
TEST(pgfx_vsync_check_runs_again_past_a_starved_display)
{
    const QxHost *const h = &S->natv.host;
    QxScreen s = {
        .w = 2,
        .h = 2,
        .depth = 8,
        .flags = QX_SCREEN_VSYNC,
    };

    CHECK(h->gfx_open(&s));
    CHECK(shown_after(h, false, true, STARVED_US) == 2);
    h->gfx_close();
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

/* The host's pages are sized to the mode, and those it cannot name given back: SCREEN 7's are
   320x200 and up to eight, text mode's 640x400 and up to two. Each was 640x480 whatever the mode,
   and kept: SCREEN 7's eight took 2.4 MB. */
size_t host_page_bytes(void);

TEST(host_pages_fit_the_mode)
{
    enum : size_t { EGA = 320 * 200, TEXT = 640 * 400 };

    CHECK(unit_typed(S, "SCREEN 7, , 7, 0") == ST_OK);
    CHECK(host_page_bytes() % EGA == 0 && host_page_bytes() >= 2 * EGA);
    CHECK(unit_typed(S, "SCREEN 0") == ST_OK);
    CHECK(host_page_bytes() % TEXT == 0 && host_page_bytes() <= 2 * TEXT);
}
