/* gfx.display's entries: the canvas and the screen it is shown on (display.bas). */
#include "gfx.h"
#include "mem.h"

enum {
    OPEN_FLAGS = GFX_DISPLAY_VSYNC | GFX_DISPLAY_NATIVE | GFX_DISPLAY_FIT | GFX_DISPLAY_STRETCH,
    SOFT_DEPTH = 8, /* software's, whatever is asked */
    RGB_MOST = 255,
};

static void close_screen(Gx *const g)
{
    if (!g->open)
        return;

    surf_close(&g->surfaces);
    qx_host->gfx_close();
    g->open = false;
}

static bool depth_known(const int32_t d) { return d == 8 || d == 15 || d == 16 || d == 32; }

void gfx_display_open(const int32_t w, const int32_t h, const int32_t depth, const int32_t flags)
{
    Gx *const g = gx_get();

    if (!g)
        return;

    if (w < 1 || h < 1 || w > GX_MOST || h > GX_MOST || !depth_known(depth) ||
        flags & ~OPEN_FLAGS) {
        gx_fail(ERR_ILLEGAL);
        return;
    }

    close_screen(g);
    QxScreen s = {
        .w = (uint16_t)w,
        .h = (uint16_t)h,
        .depth = SOFT_DEPTH,
        .flags = (uint8_t)flags,
    };

    if (!qx_host->gfx_open(&s)) {
        gx_fail(ERR_UNAVAILABLE);
        return;
    }

    uint32_t pitch;
    uint8_t *const page = qx_host->gfx_page(&pitch);

    if (!surf_open(&g->surfaces, (Pixels){page, pitch, w, h})) {
        qx_host->gfx_close();
        gx_fail(ERR_MEMORY);
        return;
    }

    g->open = true;
    g->hz = s.hz;
    g->camera = (Camera){};

    pen_reset(&g->pen, GFX_SURFACE_CANVAS);
    cmd_clear(&g->log);
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

    if (!g->open) {
        gx_fail(ERR_ILLEGAL);
        return;
    }

    qx_host->gfx_show(g->palette.rows[0]);
    uint32_t pitch;
    uint8_t *const page = qx_host->gfx_page(&pitch);
    surf_page(&g->surfaces, (Pixels){page, pitch, g->surfaces.canvas.w, g->surfaces.canvas.h});

    cmd_clear(&g->log);
}

double gfx_display_clock(void) { return gx_get() ? (double)qx_host->clock_us() / 1e6 : 0; }

float gfx_display_refresh(void)
{
    const Gx *const g = gx_get();
    return g ? g->hz : 0;
}

/* Software: no GPU, no shaders, no smoothing. */
int32_t gfx_display_caps(void) { return 0; }

/* No events reach software's host yet. */
int32_t gfx_display_poll(gfx_display_hostevent *const e)
{
    (void)e;
    return 0;
}

void gfx_display_palette(const int32_t row, const QxArray *const colours)
{
    Gx *const g = gx_get();

    if (!g || !gx_array(colours, sizeof(int32_t), 0))
        return;

    if (row < 0 || row >= PAL_ROWS) {
        gx_fail(ERR_ILLEGAL);
        return;
    }

    const uint32_t n = colours->count < PAL_COLOURS ? colours->count : PAL_COLOURS;
    pal_set(&g->palette, row, colours->data, n);
}

void gfx_display_palette_of(const int32_t row, const int32_t source)
{
    Gx *const g = gx_get();
    const Surface *const s = g ? gx_surface(g, (uint32_t)source) : nullptr;

    if (!s)
        return;

    if (row < 0 || row >= PAL_ROWS || !s->palette) {
        gx_fail(ERR_ILLEGAL);
        return;
    }

    pal_set(&g->palette, row, s->palette, PAL_COLOURS);
}

int32_t gfx_display_rgb(const int32_t r, const int32_t g_, const int32_t b)
{
    const Gx *const g = gx_get();

    if (!g)
        return 0;

    if (r < 0 || g_ < 0 || b < 0 || r > RGB_MOST || g_ > RGB_MOST || b > RGB_MOST)
        return gx_fail(ERR_ILLEGAL);

    return pal_nearest(g->palette.rows[0], (uint32_t)(r << 16 | g_ << 8 | b));
}

void gfx_display_stats(gfx_display_framestats *const s)
{
    const Gx *const g = gx_get();

    if (g)
        *s = (gfx_display_framestats){.draws = (int32_t)g->log.n};
}

int32_t gfx_display_draws(void)
{
    const Gx *const g = gx_get();
    return g ? (int32_t)g->log.n : 0;
}

void gfx_display_drawn(const int32_t n, gfx_display_command *const d)
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
        .recolor = c->recolor,
    };
}

/* Not yet: the monitors and their modes come with the hosts' players. */
int32_t gfx_display_count(void) { return gx_fail(ERR_UNAVAILABLE); }

int32_t gfx_display_modes(const int32_t monitor) { return (void)monitor, gx_fail(ERR_UNAVAILABLE); }

void gfx_display_mode(const int32_t monitor, const int32_t n, gfx_display_screenmode *const m)
{
    (void)monitor, (void)n, (void)m;
    gx_fail(ERR_UNAVAILABLE);
}

void gfx_display_fullscreen(const int32_t monitor, const int32_t n)
{
    (void)monitor, (void)n;
    gx_fail(ERR_UNAVAILABLE);
}

void gfx_display_windowed(void) { gx_fail(ERR_UNAVAILABLE); }
