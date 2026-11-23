/* gfx's state and its hooks (gfx.h). */
#include "gfx.h"
#include "mem.h"

enum { COLOURS = 256, BLENDS = GFX_PEN_BLEND | GFX_PEN_ADD };

static Gx *state;

Gx *gx_get(void)
{
    if (qx_host->minor < 1 || qx_host->size < sizeof(QxHost)) {
        gx_fail(ERR_UNAVAILABLE);
        return nullptr;
    }

    if (!state && (state = mem_alloc(sizeof *state))) {
        surf_init(&state->surfaces);
        font_init(&state->fonts);
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

/* The ink of a call's flags and the pen over a target of format, its texels a shape's colours:
   false, raised. */
static bool ink(Gx *const g, const int32_t flags, const PxFormat format, Ink *const k)
{
    const Pen *const p = &g->pen;
    Palette *const pal = &g->palette;
    const bool eight = format == PX_8;
    const BlendMode mode = flags & GFX_PEN_ADD     ? BLEND_ADD
                           : flags & GFX_PEN_BLEND ? BLEND_MIX
                                                   : BLEND_NONE;
    const uint32_t solid = pal_shade(p->tint, p->light, PAL_WHITE);
    uint8_t index = 0;
    *k = (Ink){
        .source = eight ? PX_8 : PX_32,
        .keyed = !(flags & GFX_PEN_OPAQUE),
        .solid = flags & GFX_PEN_SOLID,
        .colour = PX_OPAQUE | solid,
        .shade = pal_shade(PAL_WHITE, p->light, p->tint),
        .mode = eight ? BLEND_NONE : mode,
        .alpha = (uint8_t)p->alpha,
    };

    if (eight && !pal_lut(pal, p->recolor, p->light, p->tint, &k->lut))
        return gx_fail(ERR_MEMORY);

    if (eight && k->solid && !pal_index(pal, solid, &index))
        return gx_fail(ERR_MEMORY);

    if (eight && mode && !(k->blend = pal_blend(pal, mode, p->alpha)))
        return gx_fail(ERR_MEMORY);

    k->colour = eight ? index : k->colour;
    k->effects = k->solid || mode || (eight ? k->lut != nullptr : k->shade != PAL_WHITE);
    return true;
}

bool gx_source(Gx *const g, SoftTarget *const t, const PxFormat source)
{
    Ink *const k = &t->ink;
    const Pen *const p = &g->pen;
    const bool eight = t->px.format == PX_8;
    k->source = source;

    if (eight && source != PX_8 && !k->map && !(k->map = pal_map(&g->palette)))
        return gx_fail(ERR_MEMORY);

    if (!eight && source == PX_8 && !k->argb &&
        !(k->argb = pal_argb(&g->palette, p->recolor, p->light, p->tint)))
        return gx_fail(ERR_MEMORY);

    return true;
}

void gx_solid(const Gx *const g, SoftTarget *const t, const uint32_t colour)
{
    Ink *const k = &t->ink;
    const Pen *const p = &g->pen;
    const bool eight = t->px.format == PX_8;
    k->solid = k->effects = true;
    k->colour = eight ? (k->lut ? k->lut[colour] : colour)
                      : PX_OPAQUE | pal_shade(colour, p->light, p->tint);
}

bool gx_draw(
    Gx *const g,
    Command *const c,
    const int32_t allowed,
    SoftTarget *const t,
    Place *const where
)
{
    const Surface *const s = gx_surface(g, g->pen.target);

    if (!s)
        return false;

    const Pixels px = surf_pixels(&g->surfaces, s);
    const uint32_t most = px.format == PX_8 ? COLOURS - 1 : PAL_WHITE;

    if (c->flags & ~allowed || (c->flags & BLENDS) == BLENDS || c->colour < 0 ||
        (uint32_t)c->colour > most)
        return gx_fail(ERR_ILLEGAL);

    *where = cmd_where(&g->camera, px.w, px.h);
    cmd_place(c, &g->pen, where);

    if (!cmd_log(&g->log, c))
        return gx_fail(ERR_MEMORY);

    *t = (SoftTarget){
        .px = px,
        .clip = pen_clip(&g->pen, px.w, px.h),
        .fill = fill_formats[px.format],
    };

    return ink(g, c->flags, px.format, &t->ink);
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
