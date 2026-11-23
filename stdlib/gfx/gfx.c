/* gfx's state and its hooks (gfx.h). */
#include "gfx.h"
#include "mem.h"

enum { COLOURS = 256, BLENDS = GFX_PEN_BLEND | GFX_PEN_ADD };

static Gx *state;

Gx *gx_get(void)
{
    if (qx_host->minor < 1 || qx_host->size < offsetof(QxHost, gpu_play)) {
        gx_fail(ERR_UNAVAILABLE);
        return nullptr;
    }

    if (!state && (state = mem_alloc(sizeof *state))) {
        surf_init(&state->surfaces);
        font_init(&state->fonts);
        pal_default(&state->palette);
        pen_reset(&state->pen, GFX_SURFACE_CANVAS);
        state->backend = &backend_soft;
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

const void *gx_elements(
    const QxArray *const a,
    const uint32_t elem,
    const int32_t first,
    const int32_t n
)
{
    if (a->elem != elem || n < 0)
        return gx_fail(ERR_ILLEGAL), nullptr;

    const int64_t from = (int64_t)first - a->lbound;

    if (from < 0 || from + n > a->count)
        return gx_fail(ERR_SUBSCRIPT), nullptr;

    return (const uint8_t *)a->data + from * elem;
}

Vec *gx_points(Gx *const g, const uint32_t n)
{
    if (!mem_grow((void **)&g->points, &g->npoints, n, sizeof *g->points))
        return gx_fail(ERR_MEMORY), nullptr;

    return g->points;
}

bool gx_draw(
    Gx *const g,
    Command *const c,
    const int32_t allowed,
    Target *const t,
    Place *const where
)
{
    Surface *const s = gx_surface(g, g->pen.target);

    if (!s)
        return false;

    const uint32_t most = s->format == PX_8 ? COLOURS - 1 : PAL_WHITE;

    if (c->flags & ~allowed || (c->flags & BLENDS) == BLENDS || c->colour < 0 ||
        (uint32_t)c->colour > most)
        return gx_fail(ERR_ILLEGAL);

    *where = cmd_where(&g->camera, s->w, s->h);
    cmd_place(c, &g->pen, where);

    if (!cmd_log(&g->log, c))
        return gx_fail(ERR_MEMORY);

    t->clip = pen_clip(&g->pen, s->w, s->h);
    return g->backend->begin(g, s, c, t);
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
    font_done(&state->fonts);
    cmd_done(&state->log);
    pal_done(&state->palette);
    mem_free(state->points);
    mem_free(state);
    state = nullptr;
}
