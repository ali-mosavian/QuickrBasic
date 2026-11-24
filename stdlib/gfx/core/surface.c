/* Surfaces, views and the canvas (surface.h). */
#include "core/surface.h"
#include "core/mem.h"

void surf_init(Surfaces *const ss) { *ss = (Surfaces){}, hdl_init(&ss->t, HDL_SURFACE); }

/* s, held in slot, and its views freed. */
static void drop(Surfaces *const ss, const uint32_t h)
{
    Surface *const s = surf_get(ss, h);

    if (!s)
        return;

    for (uint32_t slot = 1; slot < ss->t.n; slot++) {
        const uint32_t v = hdl_at(&ss->t, slot);

        if (v && surf_get(ss, v)->parent == h)
            drop(ss, v);
    }

    mem_free(s->own);
    mem_free(s->palette);
    mem_free(s->depth);
    mem_free(s);
    hdl_drop(&ss->t, h);
}

void surf_done(Surfaces *const ss)
{
    for (uint32_t slot = 0; slot < ss->t.n; slot++)
        drop(ss, hdl_at(&ss->t, slot));

    hdl_done(&ss->t);
    surf_init(ss);
}

/* A surface's record, held: its handle (slot 0's when canvas), 0 no memory. */
static uint32_t hold(Surfaces *const ss, const Surface s, const bool canvas)
{
    Surface *const kept = mem_alloc(sizeof *kept);

    if (!kept)
        return 0;

    *kept = s;
    const uint32_t h =
        canvas ? (hdl_own(&ss->t, kept) ? hdl_at(&ss->t, 0) : 0) : hdl_add(&ss->t, kept);

    if (!h)
        mem_free(kept);

    return h;
}

bool surf_open(Surfaces *const ss, const Pixels page)
{
    surf_close(ss);
    ss->canvas = page;
    return hold(ss, (Surface){.w = page.w, .h = page.h, .format = page.format}, true);
}

void surf_page(Surfaces *const ss, const Pixels page) { ss->canvas = page; }

void surf_close(Surfaces *const ss)
{
    drop(ss, hdl_at(&ss->t, 0));
    ss->canvas = (Pixels){};
}

uint32_t surf_new(
    Surfaces *const ss,
    const int32_t w,
    const int32_t h,
    const PxFormat format,
    uint8_t *const own,
    uint32_t *const palette
)
{
    uint8_t *const pixels = own ? own : mem_alloc((uint32_t)(w * h) * px_bytes(format));
    const Surface s = {
        .w = w,
        .h = h,
        .format = format,
        .own = pixels,
        .palette = palette,
        .version = own != nullptr,
        .bottom = own ? h : 0,
    };

    const uint32_t handle = pixels ? hold(ss, s, false) : 0;

    if (!handle) {
        mem_free(pixels);
        mem_free(palette);
    }

    return handle;
}

uint32_t surf_view(
    Surfaces *const ss,
    const uint32_t parent,
    const int32_t x,
    const int32_t y,
    const int32_t w,
    const int32_t h
)
{
    const Surface s = {
        .parent = parent,
        .x = x,
        .y = y,
        .w = w,
        .h = h,
        .format = surf_get(ss, parent)->format,
    };

    return hold(ss, s, false);
}

Surface *surf_get(const Surfaces *const ss, const uint32_t h) { return hdl_item(&ss->t, h); }

Picture surf_picture(const Surfaces *const ss, const uint32_t h)
{
    Surface *const s = surf_get(ss, h);
    const Rect all = {0, 0, s->w, s->h};

    if (s->own)
        return (Picture){
            {s->own, (uint32_t)s->w * px_bytes(s->format), s->w, s->h, s->format},
            all,
            h,
            s->version,
            s,
        };

    if (!s->parent)
        return (Picture){ss->canvas, all, h, s->version, s};

    Picture p = surf_picture(ss, s->parent);
    p.at = (Rect){p.at.x + s->x, p.at.y + s->y, s->w, s->h};

    return p;
}

Pixels surf_pixels(const Surfaces *const ss, const Surface *const s)
{
    if (s->own)
        return (Pixels){s->own, (uint32_t)s->w * px_bytes(s->format), s->w, s->h, s->format};

    if (!s->parent)
        return ss->canvas;

    const Pixels p = surf_pixels(ss, surf_get(ss, s->parent));
    return px_cut(&p, (Rect){s->x, s->y, s->w, s->h});
}

Depth surf_depth(const Surfaces *const ss, Surface *s)
{
    int32_t x = 0, y = 0;

    for (; s->parent; s = surf_get(ss, s->parent))
        x += s->x, y += s->y;

    if (!s->depth)
        s->depth = mem_alloc((uint32_t)(s->w * s->h) * sizeof *s->depth);

    const uint32_t row = (uint32_t)s->w;
    return (Depth){s->depth ? s->depth + y * row + x : nullptr, row};
}

Surface *surf_root(const Surfaces *const ss, Surface *s, uint32_t *const h)
{
    while (s->parent) {
        *h = s->parent;
        s = surf_get(ss, s->parent);
    }

    return s;
}

void surf_wrote(const Surfaces *const ss, Surface *s, const Rect r)
{
    int32_t top = r.y;

    for (; s->parent; s = surf_get(ss, s->parent))
        top += s->y;

    const int32_t bottom = top + r.h;
    const bool none = s->top >= s->bottom;

    s->version++;
    s->top = none || top < s->top ? top : s->top;
    s->bottom = none || bottom > s->bottom ? bottom : s->bottom;
}

void surf_free(Surfaces *const ss, const uint32_t h) { drop(ss, h); }
