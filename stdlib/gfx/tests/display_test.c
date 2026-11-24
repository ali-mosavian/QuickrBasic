/* gfx.display: the canvas opened at the depth asked, the frame's commands as drawn, the flip. */
#include "plat/host/host.h"
#include "test.h"

TEST(gfx_open_gives_the_depth_asked)
{
    gfx_fresh(S);
    CHECK(CALL("display.open", I(320), I(200), I(32), I(GFX_DISPLAY_VSYNC)).error == 0);
    CHECK(CALL("surface.bits", I(SURF_SCREEN)).result.i == 32);
    CHECK(CALL("surface.width", I(SURF_SCREEN)).result.i == 320);
    CHECK(CALL("display.open", I(320), I(200), I(24), I(0)).error == 5);
    CHECK(CALL("display.open", I(0), I(200), I(8), I(0)).error == 5);
    CHECK(!CALL("display.monitors").result.i); /* no display */
}

static uint32_t asked_monitor, asked_n;
static bool refuses;

static uint32_t two_monitors(void) { return 2; }

/* Monitor 1's two modes; 2 has none. */
static uint32_t modes_of(const uint32_t monitor, QxMode *const out, const uint32_t most)
{
    static const QxMode modes[] = {{640, 480, 32, 60}, {1920, 1080, 32, 59.94F}};

    const uint32_t n = monitor == 1 ? 2 : 0;

    for (uint32_t i = 0; i < n && i < most; i++)
        out[i] = modes[i];

    return n;
}

static bool set_mode(const uint32_t monitor, const uint32_t n)
{
    asked_monitor = monitor;
    asked_n = n;
    return !refuses;
}

static const PgfxDisplay two = {.monitors = two_monitors, .modes = modes_of, .mode = set_mode};

/* The display's monitors and modes from 1; full screen and a window again only with a screen open,
   the mode one of the monitor's or 0 (the desktop's); what the display saw, polled. */
TEST(gfx_lists_modes_and_goes_full_screen)
{
    gfx_fresh(S);
    pgfx_set_display(&two);
    gfx_display_screenmode m;

    CHECK(CALL("display.monitors").result.i == 2);
    CHECK(CALL("display.modes", I(1)).result.i == 2 && !CALL("display.modes", I(2)).result.i);
    CHECK(CALL("display.modes", I(3)).error == 5 && CALL("display.modes", I(0)).error == 5);
    CHECK(!CALL("display.get_mode", I(1), I(2), REF(&m)).error);
    CHECK(m.w == 1920 && m.h == 1080 && m.bits == 32 && m.hz == 60);
    CHECK(CALL("display.get_mode", I(1), I(3), REF(&m)).error == 9);
    CHECK(CALL("display.set_fullscreen", I(1), I(0)).error == 5); /* no screen */

    CHECK(!CALL("display.open", I(320), I(200), I(8), I(0)).error);
    CHECK(!CALL("display.set_fullscreen", I(1), I(2)).error && asked_monitor == 1 && asked_n == 2);
    CHECK(CALL("display.set_fullscreen", I(1), I(3)).error == 5);
    CHECK(CALL("display.set_fullscreen", I(2), I(1)).error == 5);
    CHECK(!CALL("display.set_windowed").error && !asked_monitor && !asked_n);
    refuses = true;
    CHECK(CALL("display.set_fullscreen", I(1), I(0)).error == 73);
    refuses = false;

    gfx_display_hostevent e;
    CHECK(!CALL("display.poll", REF(&e)).result.i);
    CHECK(host_event_push(&(QxEvent){.kind = QX_EVENT_RESIZED, .w = 800, .h = 600}));

    CHECK(host_event_push(&(QxEvent){.kind = QX_EVENT_FOCUS, .focused = true}));

    CHECK(CALL("display.poll", REF(&e)).result.i == GFX_DISPLAY_RESIZED);
    CHECK(e.kind == GFX_DISPLAY_RESIZED && e.w == 800 && e.h == 600);
    CHECK(CALL("display.poll", REF(&e)).result.i == GFX_DISPLAY_FOCUS && e.focused == -1);
    CHECK(!CALL("display.poll", REF(&e)).result.i);

    CHECK(!CALL("display.set_fullscreen", I(1), I(0)).error);
    CHECK(!CALL("display.close").error && !asked_monitor); /* a window again */
    pgfx_set_display(nullptr);
}

/* Each call is logged after the camera, with the style's numbers, until the flip. */
TEST(gfx_commands_drawn)
{
    gfx_fresh(S);
    CALL("display.open", I(16), I(16), I(8), I(0));
    const int32_t s = CALL("surface.new", I(2), I(2)).result.i;
    CALL("surface.set_camera", I(SURF_SCREEN), F(10), F(-2));
    CALL("draw.rect", I(SURF_SCREEN), F(10), F(0), F(12), F(1), I(4), I(GFX_DRAW_FILL));
    CALL("draw.sprite", I(SURF_SCREEN), F(11), F(3), I(s), I(GFX_DRAW_FLIP_Y));
    CHECK(CALL("display.draws").result.i == 2);

    gfx_display_command d;
    CHECK(!CALL("display.get_drawn", I(1), REF(&d)).error);
    CHECK(d.kind == GFX_DISPLAY_DREW_RECT && d.target == SURF_SCREEN && d.colour == 4);
    CHECK(d.x == 0 && d.y == 2 && d.x2 == 2 && d.y2 == 3 && d.flags == GFX_DRAW_FILL);
    CHECK(d.light == 63 && d.alpha == 63 && d.tint == 0xFFFFFF && d.scale == 1);
    CHECK(!CALL("display.get_drawn", I(2), REF(&d)).error);
    CHECK(d.kind == GFX_DISPLAY_DREW_SPRITE && d.source == s && d.x == 1 && d.y == 5);
    CHECK(PX(0, 2) == 4 && PX(2, 3) == 4 && PX(3, 3) == 0);

    CHECK(CALL("display.get_drawn", I(3), REF(&d)).error == 9);
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
