/* gfx.surface's entries: everything drawn from or into (surface.bas). */
#include "gfx.h"
#include "image.h"
#include "mem.h"

/* w by h at x, y lies inside s. */
static bool inside(
    const Surface *const s,
    const int32_t x,
    const int32_t y,
    const int32_t w,
    const int32_t h
)
{
    return x >= 0 && y >= 0 && w >= 1 && h >= 1 && x <= s->w - w && y <= s->h - h;
}

/* h, or 0: raised Out of memory. */
static uint32_t made(const uint32_t h) { return h ? h : (uint32_t)gx_fail(ERR_MEMORY); }

int32_t gfx_surface_new(const int32_t w, const int32_t h)
{
    Gx *const g = gx_get();

    if (!g)
        return 0;

    if (w < 1 || h < 1 || w > GX_MOST || h > GX_MOST)
        return gx_fail(ERR_ILLEGAL);

    const PxFormat f = g->open ? g->surfaces.canvas.format : PX_8;
    return (int32_t)made(surf_new(&g->surfaces, w, h, f, nullptr, nullptr));
}

int32_t gfx_surface_load(const QxStr file)
{
    Gx *const g = gx_get();

    if (!g)
        return 0;

    uint32_t n;
    uint8_t *const data = qx_host->file_read(file.ptr, file.len, &n);

    if (!data)
        return gx_fail(ERR_NOT_FOUND);

    Image img;
    const ImgStatus status = img_load(data, n, &img);
    mem_free(data);

    if (status != IMG_OK)
        return gx_fail(status == IMG_NO_MEMORY ? ERR_MEMORY : ERR_ILLEGAL);

    return (int32_t)made(surf_new(&g->surfaces, img.w, img.h, img.format, img.pixels, img.palette));
}

int32_t gfx_surface_view(
    const int32_t parent,
    const int32_t x,
    const int32_t y,
    const int32_t w,
    const int32_t h
)
{
    Gx *const g = gx_get();
    const Surface *const p = g ? gx_surface(g, (uint32_t)parent) : nullptr;

    if (!p)
        return 0;

    if (!inside(p, x, y, w, h))
        return gx_fail(ERR_ILLEGAL);

    return (int32_t)made(surf_view(&g->surfaces, (uint32_t)parent, x, y, w, h));
}

int32_t gfx_surface_grid(
    const int32_t parent,
    const int32_t w,
    const int32_t h,
    const QxArray *const views
)
{
    Gx *const g = gx_get();
    const Surface *const p = g ? gx_surface(g, (uint32_t)parent) : nullptr;

    if (!p)
        return 0;

    if (!inside(p, 0, 0, w, h))
        return gx_fail(ERR_ILLEGAL);

    const int32_t across = p->w / w, down = p->h / h;

    if (!gx_array(views, sizeof(int32_t), (uint32_t)(across * down)))
        return 0;

    int32_t *const out = views->data;

    for (int32_t i = 0; i < across * down; i++) {
        const uint32_t v =
            surf_view(&g->surfaces, (uint32_t)parent, i % across * w, i / across * h, w, h);

        if (!v)
            return (int32_t)made(0);

        out[i] = (int32_t)v;
    }

    return across * down;
}

void gfx_surface_aim(const int32_t v, const int32_t x, const int32_t y)
{
    Gx *const g = gx_get();
    Surface *const s = g ? gx_surface(g, (uint32_t)v) : nullptr;

    if (!s)
        return;

    const Surface *const p = s->parent ? surf_get(&g->surfaces, s->parent) : nullptr;

    if (!p || !inside(p, x, y, s->w, s->h)) {
        gx_fail(ERR_ILLEGAL);
        return;
    }

    s->x = x;
    s->y = y;
}

void gfx_surface_anchor(const int32_t s_, const int32_t x, const int32_t y)
{
    Gx *const g = gx_get();
    Surface *const s = g ? gx_surface(g, (uint32_t)s_) : nullptr;

    if (s) {
        s->ax = x;
        s->ay = y;
    }
}

/* s's surface, null raised. */
static const Surface *any(const int32_t s)
{
    Gx *const g = gx_get();
    return g ? gx_surface(g, (uint32_t)s) : nullptr;
}

int32_t gfx_surface_width(const int32_t s_)
{
    const Surface *const s = any(s_);
    return s ? s->w : 0;
}

int32_t gfx_surface_height(const int32_t s_)
{
    const Surface *const s = any(s_);
    return s ? s->h : 0;
}

int32_t gfx_surface_bits(const int32_t s_)
{
    const Surface *const s = any(s_);
    return s ? (int32_t)px_bits(s->format) : 0;
}

/* The canvas goes with display.close. */
void gfx_surface_free(const int32_t s)
{
    Gx *const g = gx_get();

    if (!g || !gx_surface(g, (uint32_t)s))
        return;

    if ((uint32_t)s == GFX_SURFACE_CANVAS) {
        gx_fail(ERR_ILLEGAL);
        return;
    }

    g->backend->forget(g, (uint32_t)s);
    surf_free(&g->surfaces, (uint32_t)s);
}

/* s's pixels w by h at x, y, and pixels() holding their bytes, checked, current for the CPU and
   counted written when writes: s's pixels, p null raised. */
static Pixels region(
    const int32_t s_,
    const int32_t x,
    const int32_t y,
    const int32_t w,
    const int32_t h,
    const QxArray *const pixels,
    const bool writes
)
{
    Gx *const g = gx_get();
    Surface *const s = g ? gx_surface(g, (uint32_t)s_) : nullptr;

    if (!s)
        return (Pixels){};

    if (!inside(s, x, y, w, h))
        return gx_fail(ERR_ILLEGAL), (Pixels){};

    if (pixels && !gx_array(pixels, 1, (uint32_t)(w * h) * px_bytes(s->format)))
        return (Pixels){};

    if (!g->backend->sync(g, (uint32_t)s_))
        return (Pixels){};

    if (writes)
        surf_wrote(&g->surfaces, s);

    const Pixels all = surf_pixels(&g->surfaces, s);
    return px_cut(&all, (Rect){x, y, w, h});
}

/* An index at 8 bits, else the colour &HRRGGBB. */
int32_t gfx_surface_point(const int32_t s, const int32_t x, const int32_t y)
{
    const Pixels p = region(s, x, y, 1, 1, nullptr, false);
    uint32_t v = 0;

    if (!p.p)
        return 0;

    __builtin_memcpy(&v, p.p, px_bytes(p.format));
    return p.format == PX_8 ? (int32_t)v : (int32_t)(px_argb(p.format, v) & PX_WHITE);
}

/* Rows w pixels apart, in s's bytes. */
void gfx_surface_read(
    const int32_t s,
    const int32_t x,
    const int32_t y,
    const int32_t w,
    const int32_t h,
    const QxArray *const pixels
)
{
    const Pixels p = region(s, x, y, w, h, pixels, false);
    const uint32_t row = (uint32_t)w * px_bytes(p.format);

    for (int32_t r = 0; p.p && r < h; r++)
        mem_copy((uint8_t *)pixels->data + r * row, p.p + r * p.pitch, row);
}

void gfx_surface_write(
    const int32_t s,
    const int32_t x,
    const int32_t y,
    const int32_t w,
    const int32_t h,
    const QxArray *const pixels
)
{
    const Pixels p = region(s, x, y, w, h, pixels, true);
    const uint32_t row = (uint32_t)w * px_bytes(p.format);

    for (int32_t r = 0; p.p && r < h; r++)
        mem_copy(p.p + r * p.pitch, (const uint8_t *)pixels->data + r * row, row);
}
