/* gfx's display on SDL (plat/pgfx.h): the window's monitors and their full-screen modes, and what
 * the window saw, for the program.
 *
 * # How it works
 * SDL's video calls are the window's thread's: the interpreter's asks by SDL_RunOnMainThread,
 * which the loop's event poll runs, so an answer takes a frame at most. Monitors are SDL's displays
 * in its order, from 1; a mode's size is in pixels, its points times its density.
 */
#include "plat/host/host.h"
#include "plat/pgfx.h"
#include "sdl.h"

/* An ask of the window's thread: its arguments, then its answer. */
typedef struct {
    uint32_t monitor, n;
    QxMode *out;
    uint32_t most, count;
    bool ok;
} Ask;

static SDL_Window *window;
static bool quitting; /* a QUIT given the program: a second close ends the run */

/* Monitor's display: 0, none. */
static SDL_DisplayID display_of(const uint32_t monitor)
{
    int n = 0;
    SDL_DisplayID *const ids = SDL_GetDisplays(&n);
    const SDL_DisplayID id = ids && monitor >= 1 && monitor <= (uint32_t)n ? ids[monitor - 1] : 0;

    SDL_free(ids);
    return id;
}

/* Display id's monitor: 0, none. */
static uint8_t monitor_of(const SDL_DisplayID id)
{
    int n = 0;
    SDL_DisplayID *const ids = SDL_GetDisplays(&n);
    uint8_t monitor = 0;

    for (int i = 0; ids && i < n && !monitor; i++)
        if (ids[i] == id)
            monitor = (uint8_t)(i + 1);

    SDL_free(ids);
    return monitor;
}

static void count_monitors(void *const arg)
{
    int n = 0;

    SDL_free(SDL_GetDisplays(&n));
    ((Ask *)arg)->count = (uint32_t)n;
}

static void list_modes(void *const arg)
{
    Ask *const a = arg;
    int n = 0;
    SDL_DisplayMode **const modes = SDL_GetFullscreenDisplayModes(display_of(a->monitor), &n);

    for (int i = 0; modes && i < n && (uint32_t)i < a->most; i++) {
        const SDL_DisplayMode *const m = modes[i];
        a->out[i] = (QxMode){
            .w = (uint16_t)(m->w * m->pixel_density + 0.5F),
            .h = (uint16_t)(m->h * m->pixel_density + 0.5F),
            .bits = (uint8_t)SDL_BITSPERPIXEL(m->format),
            .hz = m->refresh_rate,
        };
    }

    a->count = modes ? (uint32_t)n : 0;
    SDL_free(modes);
}

/* Full screen on the monitor, in its desktop's mode (n 0) or its nth; monitor 0, a window. */
static void set_mode(void *const arg)
{
    Ask *const a = arg;

    if (!a->monitor) {
        a->ok = SDL_SetWindowFullscreen(window, false);
        return;
    }

    const SDL_DisplayID id = display_of(a->monitor);
    int n = 0;
    SDL_DisplayMode **const modes = a->n ? SDL_GetFullscreenDisplayModes(id, &n) : nullptr;
    const SDL_DisplayMode *const m = modes && a->n <= (uint32_t)n ? modes[a->n - 1] : nullptr;

    if (id && !a->n) {
        const int at = (int)SDL_WINDOWPOS_CENTERED_DISPLAY(id);
        SDL_SetWindowPosition(window, at, at);
    }

    a->ok = id && (!a->n || m) && SDL_SetWindowFullscreenMode(window, m) &&
            SDL_SetWindowFullscreen(window, true);
    SDL_free(modes);
}

/* fn run on the window's thread with a. */
static Ask asked(const SDL_MainThreadCallback fn, Ask a)
{
    if (!SDL_RunOnMainThread(fn, &a, true))
        a = (Ask){};

    return a;
}

static uint32_t monitors(void) { return asked(count_monitors, (Ask){}).count; }

static uint32_t modes(const uint32_t monitor, QxMode *const out, const uint32_t most)
{
    return asked(list_modes, (Ask){.monitor = monitor, .out = out, .most = most}).count;
}

static bool mode(const uint32_t monitor, const uint32_t n)
{
    return asked(set_mode, (Ask){.monitor = monitor, .n = n}).ok;
}

static const PgfxDisplay display = {.monitors = monitors, .modes = modes, .mode = mode};

void sdl_display_open(SDL_Window *const w)
{
    window = w;
    pgfx_set_display(&display);
}

bool sdl_display_saw(const SDL_Event *const e)
{
    switch (e->type) {
    case SDL_EVENT_QUIT:
        if (quitting || !host_event_push(&(QxEvent){.kind = QX_EVENT_QUIT}))
            return false;

        quitting = true;
        break;

    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        host_event_push(&(QxEvent){
            .kind = QX_EVENT_RESIZED,
            .w = (uint16_t)e->window.data1,
            .h = (uint16_t)e->window.data2,
        });

        break;

    case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        host_event_push(&(QxEvent){
            .kind = QX_EVENT_MOVED,
            .monitor = monitor_of((SDL_DisplayID)e->window.data1),
        });

        break;

    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        host_event_push(&(QxEvent){
            .kind = QX_EVENT_FOCUS,
            .focused = e->type == SDL_EVENT_WINDOW_FOCUS_GAINED,
        });

        break;

    default:
        break;
    }

    return true;
}
