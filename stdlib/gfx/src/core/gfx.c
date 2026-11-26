/* gfx's state and its hooks (gfx.h). */
#include "core/gfx.h"
#include "core/mem.h"

enum { COLOURS = 256, MOST = 1 << 14 }; /* MOST: pixels a side */

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
        style_reset(&state->style);
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

bool gx_illegal(const bool bad)
{
    if (bad)
        gx_fail(ERR_ILLEGAL);

    return bad;
}

bool gx_size(const int32_t w, const int32_t h)
{
    return w >= 1 && h >= 1 && w <= MOST && h <= MOST;
}

void gx_use(Gx *const g, const Backend *const b)
{
    if (g->backend == b)
        return;

    g->backend->stop(g);
    g->backend = b;
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

void gx_wrote(Gx *const g, const uint32_t h, const Rect r)
{
    const Source p = surf_source(&g->surfaces, h);
    const int32_t top = p.at.y + r.y;

    surf_wrote(&g->surfaces, surf_get(&g->surfaces, h));
    g->backend->wrote(g, p.texture, top, top + r.h);
}

bool gx_apart(Gx *const g, const uint32_t dst, const uint32_t src)
{
    uint32_t a = dst, b = src;

    surf_root(&g->surfaces, surf_get(&g->surfaces, dst), &a);
    surf_root(&g->surfaces, surf_get(&g->surfaces, src), &b);
    return !gx_illegal(a == b);
}

bool gx_check(
    Gx *const g,
    const uint32_t dst,
    const Command *const c,
    const int32_t allowed,
    Place *const where
)
{
    const Surface *const s = gx_surface(g, dst);

    if (!s || (c->source && !gx_apart(g, dst, c->source)))
        return false;

    const uint32_t most = s->format == PX_8 ? COLOURS - 1 : PAL_WHITE;

    if (gx_illegal(
            c->flags & ~allowed || (c->flags & GX_BLENDS) == GX_BLENDS || c->colour < 0 ||
            (uint32_t)c->colour > most
        ))
        return false;

    *where = cmd_where(&s->camera, s->w, s->h);
    where->clip = surf_clip(s);
    return true;
}

bool gx_begin(Gx *const g, const uint32_t dst, Command *const c, const Place *const where)
{
    c->target = dst;
    cmd_place(c, &g->style, where);

    if (!cmd_log(&g->log, c))
        return gx_fail(ERR_MEMORY);

    const Source into = surf_source(&g->surfaces, dst);
    return g->backend->begin(g, c, &into, where->clip);
}

bool gx_draw(
    Gx *const g,
    const uint32_t dst,
    Command *const c,
    const int32_t allowed,
    Place *const where
)
{
    return gx_check(g, dst, c, allowed, where) && gx_begin(g, dst, c, where);
}

void gfx_init(void) {}

void gfx_start(void) {}

void gfx_stop(void)
{
    if (!state)
        return;

    if (state->open)
        qx_host->gfx_close();

    state->backend->stop(state);

    surf_done(&state->surfaces);
    font_done(&state->fonts);
    cmd_done(&state->log);
    pal_done(&state->palette);
    mem_free(state->points);
    mem_free(state);
    state = nullptr;
}
