/* Pictures' corners (shape.h). */
#include "shape.h"
#include "gfx_qx.h"
#include "gmath.h"

/* The texels' corners, top left round, flipped as flags say. */
static void texels(const int32_t w, const int32_t h, const int32_t flags, Vertex out[4])
{
    const float l = flags & GFX_DRAW_FLIPX ? (float)w : 0, r = (float)w - l;
    const float top = flags & GFX_DRAW_FLIPY ? (float)h : 0, bottom = (float)h - top;
    out[0].u = l, out[0].v = top;
    out[1].u = r, out[1].v = top;
    out[2].u = r, out[2].v = bottom;
    out[3].u = l, out[3].v = bottom;
}

static void placed(Vertex *const v, const Place *const where, const Vec world)
{
    const Vec at = cmd_at(where, world);
    v->x = at.x;
    v->y = at.y;
}

void shape_sprite(
    const int32_t w,
    const int32_t h,
    const Vec anchor,
    const Vec a,
    const float angle,
    const float scale,
    const int32_t flags,
    const Place *const where,
    Vertex out[4]
)
{
    float s, c;
    gm_sincos(angle, &s, &c);
    const Vec edge[] = {{0, 0}, {(float)w, 0}, {(float)w, (float)h}, {0, (float)h}};

    texels(w, h, flags, out);

    for (uint32_t i = 0; i < 4; i++) {
        const float dx = (edge[i].x - 0.5f - anchor.x) * scale;
        const float dy = (edge[i].y - 0.5f - anchor.y) * scale;
        placed(&out[i], where, (Vec){a.x + c * dx - s * dy, a.y + s * dx + c * dy});
    }
}

void shape_quad(
    const int32_t w,
    const int32_t h,
    const Vec corners[4],
    const int32_t flags,
    const Place *const where,
    Vertex out[4]
)
{
    texels(w, h, flags, out);

    for (uint32_t i = 0; i < 4; i++)
        placed(&out[i], where, corners[i]);
}

void shape_segment(
    const int32_t w,
    const int32_t h,
    const Vec a,
    const Vec b,
    const float u,
    const int32_t flags,
    const Place *const where,
    Vertex out[4]
)
{
    const float run = flags & GFX_DRAW_FLIPX ? -(b.x - a.x) : b.x - a.x;
    const float u0 = flags & GFX_DRAW_FLIPX ? (float)w - u : u;
    const float top = flags & GFX_DRAW_FLIPY ? (float)h : 0, bottom = (float)h - top;
    const Vec corners[] = {a, b, {b.x, b.y + (float)h}, {a.x, a.y + (float)h}};

    const float us[] = {u0, u0 + run, u0 + run, u0}, vs[] = {top, top, bottom, bottom};

    for (uint32_t i = 0; i < 4; i++) {
        placed(&out[i], where, corners[i]);
        out[i].u = us[i];
        out[i].v = vs[i];
    }
}
