/* gfx.draw's entries: 2D into the pen's target (draw.bas). Each call is a command logged, then
   drawn at once by the software backend. */
#include "gfx.h"
#include "gmath.h"
#include "shape.h"
#include "soft/scan.h"

enum {
    BLENDS = GFX_PEN_BLEND | GFX_PEN_ADD,
    SHAPE_FLAGS = GFX_DRAW_FILL | BLENDS,
    SPRITE_FLAGS =
        GFX_DRAW_FLIPX | GFX_DRAW_FLIPY | GFX_PEN_OPAQUE | BLENDS | GFX_PEN_SOLID | GFX_PEN_SMOOTH,
    POLY_LEAST = 3,
    POLY_MOST = 32, /* the scanner's RS_MAXV, less what a clip adds */
    DEPTHS = GFX_DRAW_ZTEST | GFX_DRAW_ZSET,
    POLY_FLAGS =
        SHAPE_FLAGS | DEPTHS | GFX_DRAW_PERSP | GFX_PEN_OPAQUE | GFX_PEN_SOLID | GFX_PEN_SMOOTH,
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

    Target t;
    Place where;

    if (g && gx_draw(g, &c, 0, &t, &where))
        g->backend->clear(&t, (uint32_t)colour);
}

void gfx_draw_pset(const float x, const float y, const int32_t colour)
{
    Gx *const g = gx_get();
    Command c = drawn(GFX_DISPLAY_DREW_PSET, (Vec){x, y}, (Vec){x, y}, colour, 0);

    Target t;
    Place where;

    if (g && gx_draw(g, &c, 0, &t, &where))
        g->backend->pset(&t, (Vec){c.x, c.y}, (uint32_t)colour);
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

    Target t;
    Place where;

    if (g && gx_draw(g, &c, 0, &t, &where))
        g->backend->line(&t, (Vec){c.x, c.y}, (Vec){c.x2, c.y2}, (uint32_t)colour);
}

/* A rect turned or zoomed: a polygon of its edges, half a pixel out of its corners, or the
   outline through its corners. */
static void turned_rect(
    const Backend *const b,
    const Target *const t,
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

    if (!fill) {
        b->outline(t, corners, 4, colour);
        return;
    }

    Corner c[4];

    for (uint32_t i = 0; i < 4; i++)
        c[i] = (Corner){corners[i].x, corners[i].y, 1, 0, 0};

    b->poly(t, nullptr, c, 4, colour, 0);
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

    Target t;
    Place where;

    if (!g || !gx_draw(g, &c, SHAPE_FLAGS, &t, &where))
        return;

    const bool fill = flags & GFX_DRAW_FILL;

    if (cmd_upright(&where))
        g->backend->rect(&t, (Vec){c.x, c.y}, (Vec){c.x2, c.y2}, (uint32_t)colour, fill);
    else
        turned_rect(g->backend, &t, &where, x1, y1, x2, y2, (uint32_t)colour, fill);
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

    Target t;
    Place where;

    if (!g)
        return;

    if (!(r >= 0)) {
        gx_fail(ERR_ILLEGAL);
        return;
    }

    if (gx_draw(g, &c, SHAPE_FLAGS, &t, &where))
        g->backend
            ->circle(&t, (Vec){c.x, c.y}, r * where.zoom, (uint32_t)colour, flags & GFX_DRAW_FILL);
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
    Target t;
    Place where;

    if (gx_draw(g, &c, 0, &t, &where))
        g->backend->arc(
            &t,
            (Vec){c.x, c.y},
            r * where.zoom,
            c.angle,
            finish + where.angle,
            (uint32_t)colour
        );
}

/* x, y the first point, x2, y2 the last, after the camera; source s. A point's z counts only
   with PERSP or a depth flag, and must then be 1 or more. */
void gfx_draw_poly(
    const int32_t s_,
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

    const gfx_draw_vertex *const p = gx_elements(points, sizeof *p, first, n);

    if (!p || (s_ && !gx_surface(g, (uint32_t)s_)))
        return;

    const bool deep = flags & (DEPTHS | GFX_DRAW_PERSP);
    bool near = true;

    for (int32_t i = 0; i < n && n <= POLY_MOST; i++)
        near = near && (!deep || p[i].z >= 1);

    if (n < POLY_LEAST || n > POLY_MOST || !near) {
        gx_fail(ERR_ILLEGAL);
        return;
    }

    Command c = drawn(
        GFX_DISPLAY_DREW_POLY,
        (Vec){p[0].x, p[0].y},
        (Vec){p[n - 1].x, p[n - 1].y},
        colour,
        flags
    );

    c.source = (uint32_t)s_;
    Target t;
    Place where;

    if (!gx_draw(g, &c, POLY_FLAGS, &t, &where))
        return;

    const Backend *const b = g->backend;
    Corner placed[POLY_MOST];

    for (int32_t i = 0; i < n; i++) {
        const Vec at = cmd_at(&where, (Vec){p[i].x, p[i].y});

        placed[i] = (Corner){at.x, at.y, deep ? p[i].z : 1, p[i].u, p[i].v};
    }

    if (s_) {
        const Picture pic = surf_picture(&g->surfaces, (uint32_t)s_);

        if (b->source(g, &t, &pic))
            b->poly(&t, &pic, placed, (uint32_t)n, (uint32_t)colour, flags);

        return;
    }

    if (flags & GFX_DRAW_FILL) {
        b->poly(&t, nullptr, placed, (uint32_t)n, (uint32_t)colour, flags);
        return;
    }

    Vec *const edges = gx_points(g, (uint32_t)n);

    for (int32_t i = 0; edges && i < n; i++)
        edges[i] = (Vec){placed[i].x, placed[i].y};

    if (edges)
        b->outline(&t, edges, (uint32_t)n, (uint32_t)colour);
}

void gfx_draw_clear_depth(void)
{
    Gx *const g = gx_get();
    Surface *const s = g ? gx_surface(g, g->pen.target) : nullptr;

    if (s)
        g->backend->clear_depth(g, s);
}

/* p's picture, its anchor at a in the world (placed: at), turned by angle and scaled: upright at
   its size, a sprite; else filled through its corners. */
static void picture(
    Gx *const g,
    Target *const t,
    const Place *const where,
    const Picture *const p,
    const Vec anchor,
    const Vec a,
    const Vec at,
    const float angle,
    const float scale,
    const int32_t flags
)
{
    const Backend *const b = g->backend;

    if (!b->source(g, t, p))
        return;

    if (cmd_upright(where) && angle == 0 && scale == 1) {
        b->sprite(t, p, (Vec){at.x - anchor.x, at.y - anchor.y}, flags);

        return;
    }

    Vertex v[4];
    shape_sprite(p->at.w, p->at.h, anchor, a, angle, scale, flags, where, v);
    b->texture(t, p, v, 4, flags, false);
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
    Target t;
    Place where;

    if (!s || !gx_draw(g, &c, SPRITE_FLAGS, &t, &where))
        return;

    const Vec anchor = {(float)s->ax, (float)s->ay};

    const Picture p = surf_picture(&g->surfaces, (uint32_t)s_);

    picture(g, &t, &where, &p, anchor, (Vec){x, y}, (Vec){c.x, c.y}, angle, scale, flags);
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
    Target t;
    Place where;

    if (!gx_draw(g, &c, SPRITE_FLAGS, &t, &where))
        return;

    const Picture pic = surf_picture(&g->surfaces, (uint32_t)s_);
    const Backend *const b = g->backend;
    Vertex v[4];

    if (!b->source(g, &t, &pic))
        return;

    shape_quad(s->w, s->h, world, flags, &where, v);
    b->texture(&t, &pic, v, 3, flags, false);
    b->texture(&t, &pic, (const Vertex[]){v[0], v[2], v[3]}, 3, flags, false);
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
    Target t;
    Place where;

    if (!gx_draw(g, &c, SPRITE_FLAGS, &t, &where))
        return;

    const Picture pic = surf_picture(&g->surfaces, (uint32_t)s_);

    if (!g->backend->source(g, &t, &pic))
        return;

    for (int32_t i = 0; i + 1 < n; i++) {
        Vertex v[4];
        const Vec a = {p[i].x, p[i].y}, b = {p[i + 1].x, p[i + 1].y};

        shape_segment(s->w, s->h, a, b, p[i].x - p[0].x, flags, &where, v);
        g->backend->texture(&t, &pic, v, 4, flags, true);
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

    Target t;
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

            const Picture p = s ? surf_picture(&g->surfaces, (uint32_t)cell) : (Picture){};

            if (s)
                picture(
                    g,
                    &t,
                    &where,
                    &p,
                    (Vec){},
                    (Vec){x + dx.x, y + dx.y},
                    (Vec){c.x + dx.x, c.y + dx.y},
                    0,
                    1,
                    flags
                );
        }
}

/* The pen's font and its sheet: null, raised (its sheet freed, or no memory). */
static const Font *pen_font(Gx *const g, Picture *const sheet)
{
    const Font *const f = font_get(&g->fonts, g->pen.font);

    if (!f->sheet) {
        if (font_builtin(&g->fonts, &sheet->root)) {
            sheet->at = (Rect){0, 0, sheet->root.w, sheet->root.h};

            sheet->texture = PIC_FONT;
            sheet->version = 1;
            sheet->surface = nullptr;
            return f;
        }

        gx_fail(ERR_MEMORY);
        return nullptr;
    }

    const Surface *const s = gx_surface(g, f->sheet);

    if (s)
        *sheet = surf_picture(&g->surfaces, f->sheet);

    return s ? f : nullptr;
}

/* Each glyph a picture, in colour (SOLID's), or draw.AS_DRAWN its own texels. */
void gfx_draw_text(const float x, const float y, const QxStr s, const int32_t colour)
{
    Gx *const g = gx_get();
    const bool own = colour == GFX_DRAW_AS_DRAWN;
    Command c = drawn(GFX_DISPLAY_DREW_TEXT, (Vec){x, y}, (Vec){x, y}, own ? 0 : colour, 0);

    Picture sheet;
    const Font *const f = g ? pen_font(g, &sheet) : nullptr;
    Target t;
    Place where;

    if (!f || !gx_draw(g, &c, 0, &t, &where))
        return;

    if (!own)
        g->backend->solid(g, &t, (uint32_t)colour);

    for (uint32_t i = 0; i < s.len; i++) {
        const float dx = (float)((int32_t)i * f->w);
        Rect cell;

        if (!font_glyph(f, (uint8_t)s.ptr[i], &cell))
            continue;

        const Picture glyph = {
            sheet.root,
            {sheet.at.x + cell.x, sheet.at.y + cell.y, cell.w, cell.h},
            sheet.texture,
            sheet.version,
            sheet.surface,
        };

        picture(g, &t, &where, &glyph, (Vec){}, (Vec){x + dx, y}, (Vec){c.x + dx, c.y}, 0, 1, 0);
    }
}

int32_t gfx_draw_text_width(const QxStr s)
{
    Gx *const g = gx_get();
    return g ? (int32_t)s.len * font_get(&g->fonts, g->pen.font)->w : 0;
}

/* Its cells from the sheet's top left; those past 255 dropped. */
int32_t gfx_draw_new_font(const int32_t sheet, const int32_t w, const int32_t h)
{
    Gx *const g = gx_get();
    const Surface *const s = g ? gx_surface(g, (uint32_t)sheet) : nullptr;

    if (!s)
        return 0;

    if (w < 1 || h < 1 || w > s->w || h > s->h)
        return gx_fail(ERR_ILLEGAL);

    const uint32_t f = font_new(&g->fonts, (uint32_t)sheet, s->w, s->h, w, h);
    return f ? (int32_t)f : gx_fail(ERR_MEMORY);
}
