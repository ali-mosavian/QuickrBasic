/* gfx.display's entries: the screen, the palette and the host (display.bas). */
#include "core/gfx.h"
#include "core/mem.h"

enum {
    OPEN_FLAGS = GFX_DISPLAY_VSYNC | GFX_DISPLAY_NATIVE | GFX_DISPLAY_FIT | GFX_DISPLAY_STRETCH,
    RGB_MOST = 255,
};
static_assert(GFX_DISPLAY_VSYNC == QX_SCREEN_VSYNC);

static void close_screen(Gx *const g)
{
    if (!g->open)
        return;

    g->backend->close(g);
    surf_close(&g->surfaces);
    qx_host->gfx_close();
    g->open = false;
    gx_use(g, &backend_soft);
}

/* The host plays a GPU's lists: it gave one, and knows minor 2. */
static bool plays(const QxScreen *const s)
{
    return s->gpu && qx_host->minor >= 2 && qx_host->size >= offsetof(QxHost, gfx_monitors);
}

/* The host knows monitors, modes and events: minor 3. */
static bool displays(void) { return qx_host->minor >= 3 && qx_host->size >= sizeof(QxHost); }

/* A depth's format: false, none. */
static bool format_of(const int32_t depth, PxFormat *const f)
{
    for (PxFormat i = 0; i < PX_FORMATS; i++)
        if (px_bits(i) == (uint32_t)depth) {
            *f = i;
            return true;
        }

    return false;
}

int32_t gfx_display_open(const int32_t w, const int32_t h, const int32_t depth, const int32_t flags)
{
    Gx *const g = gx_get();

    if (!g)
        return 0;

    PxFormat format;

    if (gx_illegal(!gx_size(w, h) || !format_of(depth, &format) || flags & ~OPEN_FLAGS))
        return 0;

    close_screen(g);
    QxScreen s = {
        .w = (uint16_t)w,
        .h = (uint16_t)h,
        .depth = (uint8_t)depth,
        .flags = (uint8_t)flags,
        .shaders = 1 << QX_SHADER_MSL | 1 << QX_SHADER_ESSL | 1 << QX_SHADER_SPIRV,
    };

    if (!qx_host->gfx_open(&s) || !format_of(s.depth, &format))
        return gx_fail(ERR_UNAVAILABLE);

    gx_use(g, plays(&s) ? &backend_gpu : &backend_soft);

    if (!g->backend->open(g, &s, format)) {
        gx_use(g, &backend_soft);
        qx_host->gfx_close();
        return 0;
    }

    g->open = true;
    g->hz = s.hz;
    g->cost = (gfx_display_framestats){};

    cmd_clear(&g->log);
    return (int32_t)SURF_SCREEN;
}

void gfx_display_close(void)
{
    Gx *const g = gx_get();

    if (g)
        close_screen(g);
}

void gfx_display_flip(void)
{
    Gx *const g = gx_get();

    if (!g)
        return;

    if (gx_illegal(!g->open))
        return;

    g->backend->flip(g);

    const BackendCount n = g->backend->counted(g);
    g->cost = (gfx_display_framestats){
        .draws = (int32_t)g->log.n,
        .batches = (int32_t)n.batches,
        .uploads = (int32_t)n.uploads,
        .waits = (int32_t)n.waits,
    };

    cmd_clear(&g->log);
}

double gfx_display_clock(void) { return gx_get() ? (double)qx_host->clock_us() / 1e6 : 0; }

float gfx_display_refresh(void)
{
    const Gx *const g = gx_get();
    return g ? g->hz : 0;
}

/* No shaders yet. */
int32_t gfx_display_features(void)
{
    const Gx *const g = gx_get();
    return g ? g->backend->has : 0;
}

int32_t gfx_display_poll(gfx_display_hostevent *const e)
{
    const Gx *const g = gx_get();
    QxEvent h;

    if (!g || !g->open || !displays() || !qx_host->gfx_event(&h))
        return 0;

    *e = (gfx_display_hostevent){
        .kind = h.kind,
        .w = h.w,
        .h = h.h,
        .monitor = h.monitor,
        .focused = h.focused ? -1 : 0,
    };

    return h.kind;
}

/* The palette, colours() checked and row in range: null, raised. */
static Palette *palette_for(const int32_t row, const QxArray *const colours)
{
    Gx *const g = gx_get();

    if (!g || !gx_array(colours, sizeof(int32_t), 0) || gx_illegal(row < 0 || row >= PAL_ROWS))
        return nullptr;

    return &g->palette;
}

/* colours()'s first n up to a row's. */
static uint32_t colours_in(const QxArray *const colours)
{
    return colours->count < PAL_COLOURS ? colours->count : PAL_COLOURS;
}

void gfx_display_set_palette(const int32_t row, const QxArray *const colours)
{
    Palette *const pal = palette_for(row, colours);

    if (pal)
        pal_set(pal, row, colours->data, colours_in(colours));
}

void gfx_display_get_palette(const int32_t row, const QxArray *const colours)
{
    const Palette *const pal = palette_for(row, colours);
    int32_t *const out = colours->data;

    for (uint32_t i = 0; pal && i < colours_in(colours); i++)
        out[i] = (int32_t)(pal->rows[row][i] & PAL_WHITE);
}

void gfx_display_set_palette_from(const int32_t row, const int32_t image)
{
    Gx *const g = gx_get();
    const Surface *const s = g ? gx_surface(g, (uint32_t)image) : nullptr;

    if (!s)
        return;

    if (gx_illegal(row < 0 || row >= PAL_ROWS || !s->palette))
        return;

    pal_set(&g->palette, row, s->palette, PAL_COLOURS);
}

int32_t gfx_display_rgb(const int32_t r, const int32_t g_, const int32_t b)
{
    const Gx *const g = gx_get();

    if (!g)
        return 0;

    if (r < 0 || g_ < 0 || b < 0 || r > RGB_MOST || g_ > RGB_MOST || b > RGB_MOST)
        return gx_fail(ERR_ILLEGAL);

    const uint32_t rgb = (uint32_t)(r << 16 | g_ << 8 | b);
    return g->surfaces.screen.format == PX_8 ? pal_nearest(g->palette.rows[0], rgb) : (int32_t)rgb;
}

void gfx_display_get_stats(gfx_display_framestats *const s)
{
    const Gx *const g = gx_get();

    if (g)
        *s = g->cost;
}

int32_t gfx_display_draws(void)
{
    const Gx *const g = gx_get();
    return g ? (int32_t)g->log.n : 0;
}

void gfx_display_get_drawn(const int32_t n, gfx_display_command *const d)
{
    const Gx *const g = gx_get();

    if (!g)
        return;

    if (n < 1 || (uint32_t)n > g->log.n) {
        gx_fail(ERR_SUBSCRIPT);
        return;
    }

    const Command *const c = &g->log.items[n - 1];
    *d = (gfx_display_command){
        .kind = c->kind,
        .target = (int32_t)c->target,
        .source = (int32_t)c->source,
        .x = c->x,
        .y = c->y,
        .x2 = c->x2,
        .y2 = c->y2,
        .angle = c->angle,
        .scale = c->scale,
        .flags = c->flags,
        .colour = c->colour,
        .light = c->light,
        .tint = (int32_t)c->tint,
        .alpha = c->alpha,
        .palette_row = c->palette_row,
    };
}

int32_t gfx_display_monitors(void)
{
    return gx_get() && displays() ? (int32_t)qx_host->gfx_monitors() : 0;
}

/* monitor's modes: false, not one of the host's (an error raised). */
static bool monitor_modes(const int32_t monitor, uint32_t *const n)
{
    if (gx_illegal(monitor < 1 || monitor > gfx_display_monitors()))
        return false;

    *n = qx_host->gfx_modes((uint32_t)monitor, nullptr, 0);
    return true;
}

int32_t gfx_display_modes(const int32_t monitor)
{
    uint32_t n = 0;
    return gx_get() && monitor_modes(monitor, &n) ? (int32_t)n : 0;
}

void gfx_display_get_mode(const int32_t monitor, const int32_t n, gfx_display_screenmode *const m)
{
    uint32_t modes;

    if (!gx_get() || !monitor_modes(monitor, &modes))
        return;

    if (n < 1 || (uint32_t)n > modes) {
        gx_fail(ERR_SUBSCRIPT);
        return;
    }

    QxMode *const all = mem_alloc((uint32_t)n * sizeof *all);

    if (!all) {
        gx_fail(ERR_MEMORY);
        return;
    }

    const bool got = qx_host->gfx_modes((uint32_t)monitor, all, (uint32_t)n) >= (uint32_t)n;
    const QxMode o = all[n - 1];
    mem_free(all);

    if (!got) { /* the monitor went since */
        gx_fail(ERR_UNAVAILABLE);
        return;
    }

    *m = (gfx_display_screenmode){
        .w = o.w,
        .h = o.h,
        .bits = o.bits,
        .hz = (int32_t)(o.hz + 0.5F),
    };
}

/* The open screen on monitor, n as gfx_mode's. */
static void shown_on(const uint32_t monitor, const uint32_t n)
{
    const Gx *const g = gx_get();

    if (!g)
        return;

    if (gx_illegal(!g->open))
        return;

    if (!displays() || !qx_host->gfx_mode(monitor, n))
        gx_fail(ERR_UNAVAILABLE);
}

void gfx_display_set_fullscreen(const int32_t monitor, const int32_t n)
{
    uint32_t modes;

    if (!gx_get() || !monitor_modes(monitor, &modes))
        return;

    if (gx_illegal(n < 0 || (uint32_t)n > modes))
        return;

    shown_on((uint32_t)monitor, (uint32_t)n);
}

void gfx_display_set_windowed(void) { shown_on(0, 0); }
