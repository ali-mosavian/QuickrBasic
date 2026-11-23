/* gfx.draw's entries: 2D into the pen's target (draw.bas). Each call is a command logged, then
   drawn at once by the software backend. */
#include "gfx.h"
#include "gmath.h"
#include "shape.h"
#include "qbrt/llrm/qb/dev/host/vgadata.h"
#include "soft/scan.h"

enum {
    SPRITE_FLAGS = GFX_DRAW_FLIPX | GFX_DRAW_FLIPY | GFX_PEN_OPAQUE,
    GLYPH = 8, /* the built-in font's cells, 8 by 8 */
    POLY_LEAST = 3,
};

/* A draw call's command: a to b, upright at its size. */
static Command drawn(
    const int32_t kind,
    const Vec a,
    const Vec b,
    const int32_t colour,
    const int32_t flags
)
{
    return (Command){
        .kind = kind,
        .x = a.x,
        .y = a.y,
        .x2 = b.x,
        .y2 = b.y,
        .scale = 1,
        .colour = colour,
        .flags = flags,
    };
}

void gfx_draw_camera(const float x, const float y)
{
    Gx *const g = gx_get();

    if (g)
        g->camera = (Camera){.x = x, .y = y};
}

void gfx_draw_camera_turned(const float x, const float y, const float angle, const float zoom)
{
    Gx *const g = gx_get();

    if (!g)
        return;

    if (!(zoom > 0)) {
        gx_fail(ERR_ILLEGAL);
        return;
    }

    g->camera = (Camera){
        .x = x,
        .y = y,
        .angle = angle,
        .zoom = zoom,
        .centred = true,
    };
}

void gfx_draw_clear(const int32_t colour)
{
    Gx *const g = gx_get();
    Command c = drawn(GFX_DISPLAY_DREW_CLEAR, (Vec){}, (Vec){}, colour, 0);

    SoftTarget t;
    Place where;

    if (g && gx_draw(g, &c, 0, &t, &where))
        soft_clear(&t, (uint32_t)colour);
}

void gfx_draw_pset(const float x, const float y, const int32_t colour)
{
    Gx *const g = gx_get();
    Command c = drawn(GFX_DISPLAY_DREW_PSET, (Vec){x, y}, (Vec){x, y}, colour, 0);

    SoftTarget t;
    Place where;

    if (g && gx_draw(g, &c, 0, &t, &where))
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
    Command c = drawn(GFX_DISPLAY_DREW_LINE, (Vec){x1, y1}, (Vec){x2, y2}, colour, 0);

    SoftTarget t;
    Place where;

    if (g && gx_draw(g, &c, 0, &t, &where))
        soft_line(&t, (Vec){c.x, c.y}, (Vec){c.x2, c.y2}, (uint32_t)colour);
}

/* A rect turned or zoomed: a polygon of its edges, half a pixel out of its corners, or the
   outline through its corners. */
static void turned_rect(
    const SoftTarget *const t,
    const Place *const where,
    const float x1,
    const float y1,
    const float x2,
    const float y2,
    const uint32_t colour,
    const bool fill
)
{
    const float half = fill ? 0.5f : 0;
    const float l = (x1 < x2 ? x1 : x2) - half, r = (x1 < x2 ? x2 : x1) + half;
    const float top = (y1 < y2 ? y1 : y2) - half, bottom = (y1 < y2 ? y2 : y1) + half;
    const Vec corners[] = {
        cmd_at(where, (Vec){l, top}),
        cmd_at(where, (Vec){r, top}),
        cmd_at(where, (Vec){r, bottom}),
        cmd_at(where, (Vec){l, bottom}),
    };

    if (fill)
        soft_poly(t, corners, 4, colour);
    else
        soft_outline(t, corners, 4, colour);
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
    Command c = drawn(GFX_DISPLAY_DREW_RECT, (Vec){x1, y1}, (Vec){x2, y2}, colour, flags);

    SoftTarget t;
    Place where;

    if (!g || !gx_draw(g, &c, GFX_DRAW_FILL, &t, &where))
        return;

    const bool fill = flags & GFX_DRAW_FILL;

    if (cmd_upright(&where))
        soft_rect(&t, c.x, c.y, c.x2, c.y2, (uint32_t)colour, fill);
    else
        turned_rect(&t, &where, x1, y1, x2, y2, (uint32_t)colour, fill);
}

void gfx_draw_circle(
    const float x,
    const float y,
    const float r,
    const int32_t colour,
    const int32_t flags
)
{
    Gx *const g = gx_get();
    Command c = drawn(GFX_DISPLAY_DREW_CIRCLE, (Vec){x, y}, (Vec){x + r, y}, colour, flags);

    SoftTarget t;
    Place where;

    if (!g)
        return;

    if (!(r >= 0)) {
        gx_fail(ERR_ILLEGAL);
        return;
    }

    if (gx_draw(g, &c, GFX_DRAW_FILL, &t, &where))
        soft_circle(&t, (Vec){c.x, c.y}, r * where.zoom, (uint32_t)colour, flags & GFX_DRAW_FILL);
}

/* x2, y2 the start's point; angle the start, after the camera. */
void gfx_draw_arc(
    const float x,
    const float y,
    const float r,
    const float start,
    const float finish,
    const int32_t colour
)
{
    Gx *const g = gx_get();

    if (!g)
        return;

    if (!(r >= 0)) {
        gx_fail(ERR_ILLEGAL);
        return;
    }

    float s, co;
    gm_sincos(start, &s, &co);
    Command c = drawn(GFX_DISPLAY_DREW_ARC, (Vec){x, y}, (Vec){x + r * co, y + r * s}, colour, 0);

    c.angle = start;
    SoftTarget t;
    Place where;

    if (gx_draw(g, &c, 0, &t, &where))
        soft_arc(
            &t,
            (Vec){c.x, c.y},
            r * where.zoom,
            c.angle,
            finish + where.angle,
            (uint32_t)colour
        );
}

/* x, y the first point, x2, y2 the last, after the camera. */
void gfx_draw_poly(
    const QxArray *const points,
    const int32_t first,
    const int32_t n,
    const int32_t colour,
    const int32_t flags
)
{
    Gx *const g = gx_get();

    if (!g)
        return;

    if (n < POLY_LEAST) {
        gx_fail(ERR_ILLEGAL);
        return;
    }

    const gfx_draw_vec2 *const p = gx_elements(points, sizeof *p, first, n);
    Vec *const placed = p ? gx_points(g, (uint32_t)n) : nullptr;

    if (!placed)
        return;

    Command c = drawn(
        GFX_DISPLAY_DREW_POLY,
        (Vec){p[0].x, p[0].y},
        (Vec){p[n - 1].x, p[n - 1].y},
        colour,
        flags
    );

    SoftTarget t;
    Place where;

    if (!gx_draw(g, &c, GFX_DRAW_FILL, &t, &where))
        return;

    for (int32_t i = 0; i < n; i++)
        placed[i] = cmd_at(&where, (Vec){p[i].x, p[i].y});

    if (flags & GFX_DRAW_FILL)
        soft_poly(&t, placed, (uint32_t)n, (uint32_t)colour);
    else
        soft_outline(&t, placed, (uint32_t)n, (uint32_t)colour);
}

/* Not yet: text under a turned camera is drawn turned, with fonts as pictures. Raised: true. */
static bool upright_only(const Gx *const g)
{
    return g->camera.centred && !gx_fail(ERR_UNAVAILABLE);
}

/* s's picture, its anchor at a in the world (placed: at), turned by angle and scaled: upright at
   its size its rows are copied, else it is filled through its corners. */
static void picture(
    const Gx *const g,
    const SoftTarget *const t,
    const Place *const where,
    const Surface *const s,
    const Vec anchor,
    const Vec a,
    const Vec at,
    const float angle,
    const float scale,
    const int32_t flags
)
{
    const Pixels px = surf_pixels(&g->surfaces, s);

    if (cmd_upright(where) && angle == 0 && scale == 1) {
        soft_sprite(t, &px, at.x - anchor.x, at.y - anchor.y, flags);
        return;
    }

    Vertex v[4];
    shape_sprite(s->w, s->h, anchor, a, angle, scale, flags, where, v);
    soft_texture(t, &px, v, 4, flags, false);
}

static void sprite(
    const int32_t s_,
    const float x,
    const float y,
    const float angle,
    const float scale,
    const int32_t flags
)
{
    Gx *const g = gx_get();
    const Surface *const s = g ? gx_surface(g, (uint32_t)s_) : nullptr;
    Command c = drawn(GFX_DISPLAY_DREW_SPRITE, (Vec){x, y}, (Vec){x, y}, 0, flags);

    c.source = (uint32_t)s_;
    c.angle = angle;
    c.scale = scale;
    SoftTarget t;
    Place where;

    if (!s || !gx_draw(g, &c, SPRITE_FLAGS, &t, &where))
        return;

    const Vec anchor = {(float)s->ax, (float)s->ay};

    picture(g, &t, &where, s, anchor, (Vec){x, y}, (Vec){c.x, c.y}, angle, scale, flags);
}

void gfx_draw_sprite(const int32_t s, const float x, const float y, const int32_t flags)
{
    sprite(s, x, y, 0, 1, flags);
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
    sprite(s, x, y, angle, scale, flags);
}

/* x, y the first corner, x2, y2 the third, after the camera. Two triangles, as a GPU draws it. */
void gfx_draw_quad(
    const int32_t s_,
    const QxArray *const corners,
    const int32_t first,
    const int32_t flags
)
{
    Gx *const g = gx_get();
    const Surface *const s = g ? gx_surface(g, (uint32_t)s_) : nullptr;
    const gfx_draw_vec2 *const p = s ? gx_elements(corners, sizeof *p, first, 4) : nullptr;

    if (!p)
        return;

    const Vec world[] = {{p[0].x, p[0].y}, {p[1].x, p[1].y}, {p[2].x, p[2].y}, {p[3].x, p[3].y}};

    Command c = drawn(GFX_DISPLAY_DREW_QUAD, world[0], world[2], 0, flags);
    c.source = (uint32_t)s_;
    SoftTarget t;
    Place where;

    if (!gx_draw(g, &c, SPRITE_FLAGS, &t, &where))
        return;

    const Pixels px = surf_pixels(&g->surfaces, s);
    Vertex v[4];
    shape_quad(s->w, s->h, world, flags, &where, v);
    soft_texture(&t, &px, v, 3, flags, false);
    soft_texture(&t, &px, (const Vertex[]){v[0], v[2], v[3]}, 3, flags, false);
}

/* x, y the first point, x2, y2 the last, after the camera. The picture repeats along the points,
   their line its top edge, its texels upright: u runs with x from the first point. */
void gfx_draw_strip(
    const int32_t s_,
    const QxArray *const points,
    const int32_t first,
    const int32_t n,
    const int32_t flags
)
{
    Gx *const g = gx_get();
    const Surface *const s = g ? gx_surface(g, (uint32_t)s_) : nullptr;

    if (!s)
        return;

    if (n < 2) {
        gx_fail(ERR_ILLEGAL);
        return;
    }

    const gfx_draw_vec2 *const p = gx_elements(points, sizeof *p, first, n);

    if (!p)
        return;

    Command c = drawn(
        GFX_DISPLAY_DREW_STRIP,
        (Vec){p[0].x, p[0].y},
        (Vec){p[n - 1].x, p[n - 1].y},
        0,
        flags
    );

    c.source = (uint32_t)s_;
    SoftTarget t;
    Place where;

    if (!gx_draw(g, &c, SPRITE_FLAGS, &t, &where))
        return;

    const Pixels px = surf_pixels(&g->surfaces, s);

    for (int32_t i = 0; i + 1 < n; i++) {
        Vertex v[4];
        const Vec a = {p[i].x, p[i].y}, b = {p[i + 1].x, p[i + 1].y};

        shape_segment(s->w, s->h, a, b, p[i].x - p[0].x, flags, &where, v);
        soft_texture(&t, &px, v, 4, flags, true);
    }
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

/* The cells a turned camera may show: those under the clip's corners taken back into the world,
   a cell round them. */
static void visible_turned(
    const Place *const where,
    const Rect clip,
    const Vec origin,
    const int32_t w,
    const int32_t h,
    const int32_t columns,
    const int32_t rows,
    Rect *const cells
)
{
    const float l = (float)clip.x - 0.5f, r = l + (float)clip.w;
    const float top = (float)clip.y - 0.5f, bottom = top + (float)clip.h;
    const Vec corner[] = {{l, top}, {r, top}, {r, bottom}, {l, bottom}};

    float x0 = 0, x1 = 0, y0 = 0, y1 = 0;

    for (uint32_t i = 0; i < 4; i++) {
        const Vec p = cmd_back(where, corner[i]);
        x0 = !i || p.x < x0 ? p.x : x0;
        x1 = !i || p.x > x1 ? p.x : x1;
        y0 = !i || p.y < y0 ? p.y : y0;
        y1 = !i || p.y > y1 ? p.y : y1;
    }

    const int32_t c0 = rect_max(gm_floor((x0 - origin.x) / (float)w) - 1, 0);
    const int32_t r0 = rect_max(gm_floor((y0 - origin.y) / (float)h) - 1, 0);
    const int32_t c1 = rect_min(gm_floor((x1 - origin.x) / (float)w) + 2, columns);
    const int32_t r1 = rect_min(gm_floor((y1 - origin.y) / (float)h) + 2, rows);
    *cells = (Rect){c0, r0, c1 - c0, r1 - r0};
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
    const Vec end = {x + (float)(columns * w), y + (float)(rows * h)};

    Command c = drawn(GFX_DISPLAY_DREW_TILES, (Vec){x, y}, end, 0, flags);

    SoftTarget t;
    Place where;

    if (!gx_draw(g, &c, SPRITE_FLAGS, &t, &where))
        return;

    Rect cells;

    if (cmd_upright(&where)) {
        int32_t c0, c1, r0, r1;
        visible(scan_pixel(c.x), w, t.clip.x, t.clip.x + t.clip.w, columns, &c0, &c1);
        visible(scan_pixel(c.y), h, t.clip.y, t.clip.y + t.clip.h, rows, &r0, &r1);
        cells = (Rect){c0, r0, c1 - c0, r1 - r0};
    } else
        visible_turned(&where, t.clip, (Vec){x, y}, w, h, columns, rows, &cells);

    const int32_t *const map_cells = map->data;

    for (int32_t row = cells.y; row < cells.y + cells.h; row++)
        for (int32_t col = cells.x; col < cells.x + cells.w; col++) {
            const int32_t cell = map_cells[row * columns + col];
            const Surface *const s = cell ? gx_surface(g, (uint32_t)cell) : nullptr;

            if (cell && !s)
                return;

            const Vec dx = {(float)(col * w), (float)(row * h)};

            if (s)
                picture(
                    g,
                    &t,
                    &where,
                    s,
                    (Vec){},
                    (Vec){x + dx.x, y + dx.y},
                    (Vec){c.x + dx.x, c.y + dx.y},
                    0,
                    1,
                    flags
                );
        }
}

void gfx_draw_text(const float x, const float y, const QxStr s, const int32_t colour)
{
    Gx *const g = gx_get();
    Command c = drawn(GFX_DISPLAY_DREW_TEXT, (Vec){x, y}, (Vec){x, y}, colour, 0);

    SoftTarget t;
    Place where;

    if (!g || upright_only(g) || !gx_draw(g, &c, 0, &t, &where))
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

int32_t gfx_draw_new_font(const int32_t sheet, const int32_t w, const int32_t h)
{
    (void)sheet, (void)w, (void)h;
    return gx_fail(ERR_UNAVAILABLE);
}
