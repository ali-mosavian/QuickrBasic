/* gfx.draw's entries: 2D into the pen's target (draw.bas). Each call is a command logged, then
   drawn at once by the software backend. */
#include "gfx.h"
#include "qbrt/llrm/qb/dev/host/vgadata.h"
#include "soft/scan.h"

enum {
    SPRITE_FLAGS = GFX_DRAW_FLIPX | GFX_DRAW_FLIPY | GFX_PEN_OPAQUE,
    GLYPH = 8, /* the built-in font's cells, 8 by 8 */
};

void gfx_draw_camera(const float x, const float y)
{
    Gx *const g = gx_get();

    if (g)
        g->camera = (Camera){x, y};
}

void gfx_draw_clear(const int32_t colour)
{
    Gx *const g = gx_get();
    Command c = {.kind = GFX_DISPLAY_DREW_CLEAR, .colour = colour};

    SoftTarget t;

    if (g && gx_draw(g, &c, 0, &t))
        soft_clear(&t, (uint32_t)colour);
}

void gfx_draw_pset(const float x, const float y, const int32_t colour)
{
    Gx *const g = gx_get();
    Command c = {
        .kind = GFX_DISPLAY_DREW_PSET,
        .x = x,
        .y = y,
        .x2 = x,
        .y2 = y,
        .colour = colour,
    };

    SoftTarget t;

    if (g && gx_draw(g, &c, 0, &t))
        soft_pset(&t, c.x, c.y, (uint32_t)colour);
}

void gfx_draw_line(
    const float x1,
    const float y1,
    const float x2,
    const float y2,
    const int32_t colour
)
{
    Gx *const g = gx_get();
    Command c = {
        .kind = GFX_DISPLAY_DREW_LINE,
        .x = x1,
        .y = y1,
        .x2 = x2,
        .y2 = y2,
        .colour = colour,
    };

    SoftTarget t;

    if (g && gx_draw(g, &c, 0, &t))
        soft_line(&t, c.x, c.y, c.x2, c.y2, (uint32_t)colour);
}

void gfx_draw_rect(
    const float x1,
    const float y1,
    const float x2,
    const float y2,
    const int32_t colour,
    const int32_t flags
)
{
    Gx *const g = gx_get();
    Command c = {
        .kind = GFX_DISPLAY_DREW_RECT,
        .x = x1,
        .y = y1,
        .x2 = x2,
        .y2 = y2,
        .colour = colour,
        .flags = flags,
    };

    SoftTarget t;

    if (g && gx_draw(g, &c, GFX_DRAW_FILL, &t))
        soft_rect(&t, c.x, c.y, c.x2, c.y2, (uint32_t)colour, flags & GFX_DRAW_FILL);
}

void gfx_draw_sprite(const int32_t s_, const float x, const float y, const int32_t flags)
{
    Gx *const g = gx_get();
    const Surface *const s = g ? gx_surface(g, (uint32_t)s_) : nullptr;
    Command c = {
        .kind = GFX_DISPLAY_DREW_SPRITE,
        .source = (uint32_t)s_,
        .x = x,
        .y = y,
        .x2 = x,
        .y2 = y,
        .flags = flags,
    };

    SoftTarget t;

    if (!s || !gx_draw(g, &c, SPRITE_FLAGS, &t))
        return;

    const Pixels px = surf_pixels(&g->surfaces, s);
    soft_sprite(&t, &px, c.x - (float)s->ax, c.y - (float)s->ay, flags);
}

/* The cells, of size pixels from first, that meet [lo, hi): from *from up to *to, of n. */
static void visible(
    const int32_t first,
    const int32_t size,
    const int32_t lo,
    const int32_t hi,
    const int32_t n,
    int32_t *const from,
    int32_t *const to
)
{
    const int32_t a = lo - first, b = hi - first;
    *from = rect_max(a >= 0 ? a / size : -((size - 1 - a) / size), 0);
    *to = rect_min(b > 0 ? (b + size - 1) / size : 0, n);
}

void gfx_draw_tiles(
    const QxArray *const map,
    const int32_t columns,
    const float x,
    const float y,
    const int32_t w,
    const int32_t h,
    const int32_t flags
)
{
    Gx *const g = gx_get();

    if (!g || !gx_array(map, sizeof(int32_t), 0))
        return;

    if (columns < 1 || w < 1 || h < 1) {
        gx_fail(ERR_ILLEGAL);
        return;
    }

    const int32_t rows = (int32_t)map->count / columns;
    Command c = {
        .kind = GFX_DISPLAY_DREW_TILES,
        .x = x,
        .y = y,
        .x2 = x + (float)(columns * w),
        .y2 = y + (float)(rows * h),
        .flags = flags,
    };

    SoftTarget t;

    if (!gx_draw(g, &c, SPRITE_FLAGS, &t))
        return;

    const int32_t left = scan_pixel(c.x), top = scan_pixel(c.y);
    const int32_t *const cells = map->data;
    int32_t c0, c1, r0, r1;
    visible(left, w, t.clip.x, t.clip.x + t.clip.w, columns, &c0, &c1);
    visible(top, h, t.clip.y, t.clip.y + t.clip.h, rows, &r0, &r1);

    for (int32_t row = r0; row < r1; row++)
        for (int32_t col = c0; col < c1; col++) {
            const int32_t cell = cells[row * columns + col];
            const Surface *const s = cell ? gx_surface(g, (uint32_t)cell) : nullptr;

            if (cell && !s)
                return;

            if (s) {
                const Pixels px = surf_pixels(&g->surfaces, s);
                soft_sprite(&t, &px, c.x + (float)(col * w), c.y + (float)(row * h), flags);
            }
        }
}

void gfx_draw_text(const float x, const float y, const QxStr s, const int32_t colour)
{
    Gx *const g = gx_get();
    Command c = {
        .kind = GFX_DISPLAY_DREW_TEXT,
        .x = x,
        .y = y,
        .x2 = x,
        .y2 = y,
        .colour = colour,
    };

    SoftTarget t;

    if (!g || !gx_draw(g, &c, 0, &t))
        return;

    for (uint32_t i = 0; i < s.len; i++) {
        const uint8_t code = (uint8_t)s.ptr[i];
        soft_glyph(
            &t,
            qb_vga_font_8 + code * GLYPH,
            GLYPH,
            c.x + (float)(i * GLYPH),
            c.y,
            (uint32_t)colour
        );
    }
}

int32_t gfx_draw_text_width(const QxStr s) { return gx_get() ? (int32_t)(s.len * GLYPH) : 0; }

/* Not yet: turned and textured shapes come with the rest of 2D. */
void gfx_draw_camera_turned(const float x, const float y, const float angle, const float zoom)
{
    (void)x, (void)y, (void)angle, (void)zoom;
    gx_fail(ERR_UNAVAILABLE);
}

void gfx_draw_circle(
    const float x,
    const float y,
    const float r,
    const int32_t colour,
    const int32_t flags
)
{
    (void)x, (void)y, (void)r, (void)colour, (void)flags;
    gx_fail(ERR_UNAVAILABLE);
}

void gfx_draw_arc(
    const float x,
    const float y,
    const float r,
    const float start,
    const float finish,
    const int32_t colour
)
{
    (void)x, (void)y, (void)r, (void)start, (void)finish, (void)colour;
    gx_fail(ERR_UNAVAILABLE);
}

void gfx_draw_poly(
    const QxArray *const points,
    const int32_t first,
    const int32_t n,
    const int32_t colour,
    const int32_t flags
)
{
    (void)points, (void)first, (void)n, (void)colour, (void)flags;
    gx_fail(ERR_UNAVAILABLE);
}

void gfx_draw_sprite_turned(
    const int32_t s,
    const float x,
    const float y,
    const float angle,
    const float scale,
    const int32_t flags
)
{
    (void)s, (void)x, (void)y, (void)angle, (void)scale, (void)flags;
    gx_fail(ERR_UNAVAILABLE);
}

void gfx_draw_quad(
    const int32_t s,
    const QxArray *const corners,
    const int32_t first,
    const int32_t flags
)
{
    (void)s, (void)corners, (void)first, (void)flags;
    gx_fail(ERR_UNAVAILABLE);
}

void gfx_draw_strip(
    const int32_t s,
    const QxArray *const points,
    const int32_t first,
    const int32_t n,
    const int32_t flags
)
{
    (void)s, (void)points, (void)first, (void)n, (void)flags;
    gx_fail(ERR_UNAVAILABLE);
}

int32_t gfx_draw_new_font(const int32_t sheet, const int32_t w, const int32_t h)
{
    (void)sheet, (void)w, (void)h;
    return gx_fail(ERR_UNAVAILABLE);
}
