/* gfx's state and its hooks (gfx.h). */
#include "gfx.h"
#include "mem.h"

enum {
    /* the flags a later step draws: not yet */
    LATER = GFX_PEN_BLEND | GFX_PEN_ADD | GFX_PEN_SOLID | GFX_PEN_SMOOTH,
    COLOURS = 256,
};

static Gx *state;

Gx *gx_get(void)
{
    if (qx_host->minor < 1 || qx_host->size < sizeof(QxHost)) {
        gx_fail(ERR_UNAVAILABLE);
        return nullptr;
    }

    if (!state && (state = mem_alloc(sizeof *state))) {
        surf_init(&state->surfaces);
        pal_default(&state->palette);
        pen_reset(&state->pen, GFX_SURFACE_CANVAS);
    }

    if (!state)
        gx_fail(ERR_MEMORY);

    return state;
}

bool gx_fail(const uint8_t code)
{
    qx_fail(code);
    return false;
}

Surface *gx_surface(Gx *const g, const uint32_t h)
{
    Surface *const s = surf_get(&g->surfaces, h);

    if (!s)
        gx_fail(ERR_ILLEGAL);

    return s;
}

bool gx_array(const QxArray *const a, const uint32_t elem, const uint32_t need)
{
    if (a->elem != elem)
        return gx_fail(ERR_ILLEGAL);

    return a->count >= need || gx_fail(ERR_SUBSCRIPT);
}

bool gx_draw(Gx *const g, Command *const c, const int32_t allowed, SoftTarget *const t)
{
    const int32_t refused = c->flags & ~allowed;

    if (refused)
        return gx_fail(refused & ~LATER ? ERR_ILLEGAL : ERR_UNAVAILABLE);

    if (c->colour < 0 || c->colour >= COLOURS)
        return gx_fail(ERR_ILLEGAL);

    cmd_place(c, &g->pen, &g->camera);
    const Surface *const s = gx_surface(g, c->target);

    if (!s)
        return false;

    if (!cmd_log(&g->log, c))
        return gx_fail(ERR_MEMORY);

    const Pixels px = surf_pixels(&g->surfaces, s);
    *t = (SoftTarget){px, pen_clip(&g->pen, px.w, px.h), fill_formats[FILL_8]};

    return true;
}

void gfx_init(void) {}

void gfx_start(void) {}

void gfx_stop(void)
{
    if (!state)
        return;

    if (state->open)
        qx_host->gfx_close();

    surf_done(&state->surfaces);
    cmd_done(&state->log);
    mem_free(state);
    state = nullptr;
}
