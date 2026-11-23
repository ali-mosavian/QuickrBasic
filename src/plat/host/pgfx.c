/* gfx's screen in memory: the terminal's, the window's and the browser's (plat/pgfx.h). */
#include "plat/pgfx.h"
#include "plat/host/host.h"
#include "plat/pmem.h"
#include <stdatomic.h>
#include <string.h>

enum { WAIT_US = 100000, MILLI = 1000 };

static struct {
    const PgfxPlayer *player, *playing; /* the display's; the open screen's */
    const PgfxDisplay *display;
    uint8_t *page, *shown; /* w by h each, null none open */
    uint16_t w, h;
    uint8_t depth, bytes; /* bytes a pixel */
    bool showing, vsync;
    bool full; /* shown full screen */
    uint32_t palette[256];
} screen;

static HostEvents events;

HostEvents *host_events(void) { return &events; }

bool host_event_push(const QxEvent *const e)
{
    const uint32_t at = atomic_load(&events.pushed);

    if (at - atomic_load(&events.read) == HOST_EVENTS ||
        (e->kind == QX_EVENT_QUIT && !atomic_load(&events.asked)))
        return false;

    events.items[at % HOST_EVENTS] = *e;
    atomic_store(&events.pushed, at + 1);
    return true;
}

/* A screen opened: what the display saw before is not its. */
static void opened(const bool vsync)
{
    screen.vsync = vsync;
    atomic_store(&events.asked, false);
    atomic_store(&events.read, atomic_load(&events.pushed));
}

/* A depth's bytes a pixel: 0, none shown. */
static uint8_t bytes_of(const uint8_t depth)
{
    return depth == 8 ? 1 : depth == 15 || depth == 16 ? 2 : depth == 32 ? 4 : 0;
}

/* A frame given the display: with vsync, back once it shows one after the one it shows now. */
static void synced(void)
{
    if (screen.vsync)
        host_frame_wait(atomic_load(&host_display()->frames), WAIT_US);
}

void pgfx_set_player(const PgfxPlayer *const p) { screen.player = p; }

void pgfx_set_display(const PgfxDisplay *const d) { screen.display = d; }

const PgfxPlayer *pgfx_open_played(
    const uint16_t w,
    const uint16_t h,
    const uint8_t shaders,
    const bool vsync
)
{
    pgfx_close();

    if (screen.player && screen.player->shaders & shaders && screen.player->open(w, h)) {
        screen.playing = screen.player;
        opened(vsync);
    }

    return screen.playing;
}

const PgfxPlayer *pgfx_playing(void) { return screen.playing; }

bool pgfx_open(const uint16_t w, const uint16_t h, const uint8_t depth, const bool vsync)
{
    pgfx_close();

    if (!bytes_of(depth) || !w || !h)
        return false;

    const size_t bytes = (size_t)w * h * bytes_of(depth);
    uint8_t *const page = pmem_resize(nullptr, bytes), *const shown = pmem_resize(nullptr, bytes);

    if (!page || !shown) {
        pmem_free(page);
        pmem_free(shown);
        return false;
    }

    memset(page, 0, bytes);
    screen.page = page;
    screen.shown = shown;
    screen.w = w;
    screen.h = h;
    screen.depth = depth;
    screen.bytes = bytes_of(depth);
    opened(vsync);
    return true;
}

void pgfx_close(void)
{
    if (screen.full)
        screen.display->mode(0, 0);

    screen.full = false;
    atomic_store(&events.asked, false);

    if (screen.playing)
        screen.playing->close();

    screen.playing = nullptr;
    pmem_free(screen.page);
    pmem_free(screen.shown);
    screen.page = screen.shown = nullptr;
    screen.showing = screen.vsync = false;
}

uint8_t *pgfx_page(uint32_t *const pitch)
{
    *pitch = (uint32_t)screen.w * screen.bytes;
    return screen.page;
}

void pgfx_show(const uint32_t *const palette)
{
    if (!screen.page)
        return;

    memcpy(screen.shown, screen.page, (size_t)screen.w * screen.h * screen.bytes);
    memcpy(screen.palette, palette, sizeof screen.palette);
    screen.showing = true;
    synced();
}

void pgfx_play(const uint32_t *const list, const uint32_t words)
{
    if (screen.playing && screen.playing->play(list, words))
        synced();
}

float pgfx_hz(void) { return (float)atomic_load(&host_display()->millihz) / MILLI; }

bool pgfx_shown(PgfxFrame *const f)
{
    if (!screen.showing)
        return false;

    *f = (PgfxFrame){screen.shown, screen.w, screen.h, screen.depth, screen.palette};

    return true;
}

uint32_t pgfx_monitors(void) { return screen.display ? screen.display->monitors() : 0; }

uint32_t pgfx_modes(const uint32_t monitor, QxMode *const out, const uint32_t most)
{
    return screen.display ? screen.display->modes(monitor, out, most) : 0;
}

bool pgfx_mode(const uint32_t monitor, const uint32_t n)
{
    if (!screen.display || !(screen.page || screen.playing) || !screen.display->mode(monitor, n))
        return false;

    screen.full = monitor != 0;
    return true;
}

bool pgfx_event(QxEvent *const e)
{
    atomic_store(&events.asked, true);
    const uint32_t at = atomic_load(&events.read);

    if (at == atomic_load(&events.pushed))
        return false;

    *e = events.items[at % HOST_EVENTS];
    atomic_store(&events.read, at + 1);
    return true;
}
