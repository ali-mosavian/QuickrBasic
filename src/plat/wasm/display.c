/* gfx's display on the page (plat/pgfx.h): one monitor, its one mode the screen's, which the page
 * writes (host.h's HostDisplay); full screen by the Fullscreen API (web/main.mjs).
 */
#include <stdatomic.h>
#include <stddef.h>

#include "plat/host/host.h"
#include "plat/pgfx.h"
#include "wasm.h"

enum { BITS = 32, MILLI = 1000 };

/* The page's Irq.event writes these. */
static_assert(sizeof(QxEvent) == 8 && offsetof(HostEvents, pushed) == HOST_EVENTS * 8);

static uint32_t monitors(void) { return atomic_load(&host_display()->w) ? 1 : 0; }

static uint32_t modes(const uint32_t monitor, QxMode *const out, const uint32_t most)
{
    HostDisplay *const d = host_display();
    const uint32_t w = atomic_load(&d->w);

    if (monitor != 1 || !w)
        return 0;

    if (most)
        out[0] = (QxMode){
            .w = (uint16_t)w,
            .h = (uint16_t)atomic_load(&d->h),
            .bits = BITS,
            .hz = (float)atomic_load(&d->millihz) / MILLI,
        };

    return 1;
}

static bool mode(const uint32_t monitor, const uint32_t n)
{
    (void)n;
    js_fullscreen(monitor != 0);
    return true;
}

static const PgfxDisplay display = {.monitors = monitors, .modes = modes, .mode = mode};

[[gnu::constructor]]
static void display_set(void)
{
    pgfx_set_display(&display);
}
