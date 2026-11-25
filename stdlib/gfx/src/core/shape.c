/* Sprites' corners (shape.h). */
#include "core/shape.h"
#include "gfx_qx.h"
#include "base/gmath.h"

/* The texels' corners, top left round, flipped as flags say. */
static void texels(const int32_t w, const int32_t h, const int32_t flags, Corner out[4])
{
    const float l = flags & GFX_DRAW_FLIP_X ? (float)w : 0, r = (float)w - l;
    const float top = flags & GFX_DRAW_FLIP_Y ? (float)h : 0, bottom = (float)h - top;
    out[0].u = l, out[0].v = top;
    out[1].u = r, out[1].v = top;
    out[2].u = r, out[2].v = bottom;
    out[3].u = l, out[3].v = bottom;
}

static void placed(Corner *const v, const Place *const where, const Vec world)
{
    const Vec at = cmd_at(where, world);
    v->x = at.x;
    v->y = at.y;
    v->z = 1;
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
    Corner out[4]
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

Turn shape_turn(
    const int32_t w,
    const int32_t h,
    const Vec anchor,
    const Vec a,
    const float angle,
    const float scale,
    const int32_t flags,
    const Place *const where
)
{
    float s, c;
    gm_sincos(angle, &s, &c);
    const float dx = (-0.5f - anchor.x) * scale, dy = (-0.5f - anchor.y) * scale;
    const float cx = c * scale, sx = s * scale;
    Turn t = {
        .at = cmd_at(where, (Vec){a.x + c * dx - s * dy, a.y + s * dx + c * dy}),
        .across = {where->m11 * cx + where->m12 * sx, where->m21 * cx + where->m22 * sx},
        .down = {where->m12 * cx - where->m11 * sx, where->m22 * cx - where->m21 * sx},
    };

    if (flags & GFX_DRAW_FLIP_X) {
        t.at = (Vec){t.at.x + (float)w * t.across.x, t.at.y + (float)w * t.across.y};
        t.across = (Vec){-t.across.x, -t.across.y};
    }

    if (flags & GFX_DRAW_FLIP_Y) {
        t.at = (Vec){t.at.x + (float)h * t.down.x, t.at.y + (float)h * t.down.y};
        t.down = (Vec){-t.down.x, -t.down.y};
    }

    return t;
}

void turn_corners(const Turn *const t, const int32_t w, const int32_t h, Corner out[4])
{
    const float fw = (float)w, fh = (float)h;
    const Vec a = {fw * t->across.x, fw * t->across.y}, d = {fh * t->down.x, fh * t->down.y};

    out[0] = (Corner){.x = t->at.x, .y = t->at.y, .z = 1, .u = 0, .v = 0};
    out[1] = (Corner){.x = t->at.x + a.x, .y = t->at.y + a.y, .z = 1, .u = fw, .v = 0};
    out[2] = (Corner){.x = t->at.x + a.x + d.x, .y = t->at.y + a.y + d.y, .z = 1, .u = fw, .v = fh};
    out[3] = (Corner){.x = t->at.x + d.x, .y = t->at.y + d.y, .z = 1, .u = 0, .v = fh};
}

void shape_quad(
    const int32_t w,
    const int32_t h,
    const Vec corners[4],
    const int32_t flags,
    const Place *const where,
    Corner out[4]
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
    Corner out[4]
)
{
    const float run = flags & GFX_DRAW_FLIP_X ? -(b.x - a.x) : b.x - a.x;
    const float u0 = flags & GFX_DRAW_FLIP_X ? (float)w - u : u;
    const float top = flags & GFX_DRAW_FLIP_Y ? (float)h : 0, bottom = (float)h - top;
    const Vec corners[] = {a, b, {b.x, b.y + (float)h}, {a.x, a.y + (float)h}};

    const float us[] = {u0, u0 + run, u0 + run, u0}, vs[] = {top, top, bottom, bottom};

    for (uint32_t i = 0; i < 4; i++) {
        placed(&out[i], where, corners[i]);
        out[i].u = us[i];
        out[i].v = vs[i];
    }
}
