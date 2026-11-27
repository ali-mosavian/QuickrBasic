/* gfx.draw's entries: 2D into a surface (draw.bas). Each call is a command logged, then drawn
   through the screen's backend. */
#include "core/gfx.h"
#include "base/gmath.h"
#include "core/shape.h"
#include "base/cover.h"

enum {
    SHAPE_FLAGS = GFX_DRAW_FILL | GX_BLENDS,
    SPRITE_FLAGS = GFX_DRAW_FLIP_X | GFX_DRAW_FLIP_Y | GFX_DRAW_OPAQUE | GX_BLENDS |
                   GFX_DRAW_SOLID | GFX_DRAW_SMOOTH,
    POLY_LEAST = 3,
    POLY_MOST = 32, /* the scanner's SCAN_MAXV, less what a clip adds */
    DEPTHS = GFX_DRAW_DEPTH_TEST | GFX_DRAW_DEPTH_WRITE,
    POLY_FLAGS = SHAPE_FLAGS | DEPTHS | GFX_DRAW_PERSPECTIVE | GFX_DRAW_OPAQUE | GFX_DRAW_SOLID |
                 GFX_DRAW_SMOOTH,
};
static_assert(!((SPRITE_FLAGS | POLY_FLAGS) & POLY_WRAP));

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

void gfx_draw_clear(const int32_t dst, const int32_t colour)
{
    Gx *const g = gx_get();
    Command c = drawn(GFX_DISPLAY_DREW_CLEAR, (Vec){}, (Vec){}, colour, 0);

    Place where;

    if (g && gx_draw(g, (uint32_t)dst, &c, 0, &where))
        g->backend->clear(g, (uint32_t)colour);
}

void gfx_draw_pset(const int32_t dst, const float x, const float y, const int32_t colour)
{
    Gx *const g = gx_get();
    Command c = drawn(GFX_DISPLAY_DREW_PSET, (Vec){x, y}, (Vec){x, y}, colour, 0);

    Place where;

    if (g && gx_draw(g, (uint32_t)dst, &c, 0, &where))
        g->backend->pset(g, (Vec){c.x, c.y}, (uint32_t)colour);
}

void gfx_draw_line(
    const int32_t dst,
    const float x1,
    const float y1,
    const float x2,
    const float y2,
    const int32_t colour
)
{
    Gx *const g = gx_get();
    Command c = drawn(GFX_DISPLAY_DREW_LINE, (Vec){x1, y1}, (Vec){x2, y2}, colour, 0);

    Place where;

    if (g && gx_draw(g, (uint32_t)dst, &c, 0, &where))
        g->backend->line(g, (Vec){c.x, c.y}, (Vec){c.x2, c.y2}, (uint32_t)colour);
}

/* A rect turned or zoomed: a polygon of its edges, half a pixel out of its corners, or the
   outline through its corners. */
static void turned_rect(
    Gx *const g,
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
        g->backend->outline(g, corners, 4, colour);
        return;
    }

    Corner c[4];

    for (uint32_t i = 0; i < 4; i++)
        c[i] = (Corner){corners[i].x, corners[i].y, 1, 0, 0};

    g->backend->poly(g, nullptr, c, 4, colour, 0);
}

void gfx_draw_rect(
    const int32_t dst,
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

    Place where;

    if (!g || !gx_draw(g, (uint32_t)dst, &c, SHAPE_FLAGS, &where))
        return;

    const bool fill = flags & GFX_DRAW_FILL;

    if (cmd_upright(&where))
        g->backend->rect(g, (Vec){c.x, c.y}, (Vec){c.x2, c.y2}, (uint32_t)colour, fill);
    else
        turned_rect(g, &where, x1, y1, x2, y2, (uint32_t)colour, fill);
}

void gfx_draw_circle(
    const int32_t dst,
    const float x,
    const float y,
    const float r,
    const int32_t colour,
    const int32_t flags
)
{
    Gx *const g = gx_get();
    Command c = drawn(GFX_DISPLAY_DREW_CIRCLE, (Vec){x, y}, (Vec){x + r, y}, colour, flags);

    Place where;

    if (g && !gx_illegal(!(r >= 0)) && gx_draw(g, (uint32_t)dst, &c, SHAPE_FLAGS, &where))
        g->backend
            ->circle(g, (Vec){c.x, c.y}, r * where.zoom, (uint32_t)colour, flags & GFX_DRAW_FILL);
}

/* x2, y2 the start's point; angle the start, after the camera. */
void gfx_draw_arc(
    const int32_t dst,
    const float x,
    const float y,
    const float r,
    const float start,
    const float finish,
    const int32_t colour
)
{
    Gx *const g = gx_get();

    if (!g || gx_illegal(!(r >= 0)))
        return;

    float s, co;
    gm_sincos(start, &s, &co);
    Command c = drawn(GFX_DISPLAY_DREW_ARC, (Vec){x, y}, (Vec){x + r * co, y + r * s}, colour, 0);

    c.angle = start;
    Place where;

    if (gx_draw(g, (uint32_t)dst, &c, 0, &where))
        g->backend->arc(
            g,
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
    const int32_t dst,
    const QxArray *const points,
    const int32_t first,
    const int32_t n,
    const int32_t s_,
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

    const bool deep = flags & (DEPTHS | GFX_DRAW_PERSPECTIVE);
    bool near = true;

    for (int32_t i = 0; i < n && n <= POLY_MOST; i++)
        near = near && (!deep || p[i].z >= 1);

    if (gx_illegal(n < POLY_LEAST || n > POLY_MOST || !near))
        return;

    Command c = drawn(
        GFX_DISPLAY_DREW_POLY,
        (Vec){p[0].x, p[0].y},
        (Vec){p[n - 1].x, p[n - 1].y},
        colour,
        flags
    );

    c.source = (uint32_t)s_;
    Place where;

    if (!gx_draw(g, (uint32_t)dst, &c, POLY_FLAGS, &where))
        return;

    const Backend *const b = g->backend;
    Corner placed[POLY_MOST];

    for (int32_t i = 0; i < n; i++) {
        const Vec at = cmd_at(&where, (Vec){p[i].x, p[i].y});

        placed[i] = (Corner){at.x, at.y, deep ? p[i].z : 1, p[i].u, p[i].v};
    }

    if (s_) {
        const Source src = surf_source(&g->surfaces, (uint32_t)s_);

        if (b->source(g, &src))
            b->poly(g, &src, placed, (uint32_t)n, (uint32_t)colour, flags);

        return;
    }

    if (flags & GFX_DRAW_FILL) {
        b->poly(g, nullptr, placed, (uint32_t)n, (uint32_t)colour, flags);
        return;
    }

    Vec *const edges = gx_points(g, (uint32_t)n);

    for (int32_t i = 0; edges && i < n; i++)
        edges[i] = (Vec){placed[i].x, placed[i].y};

    if (edges)
        b->outline(g, edges, (uint32_t)n, (uint32_t)colour);
}

/* v names a pixel's centre. */
static bool whole(const Vec v)
{
    return (float)gm_floor(v.x) == v.x && (float)gm_floor(v.y) == v.y;
}

/* Source p, its anchor at a in the world (placed: at), turned by angle and scaled: upright at
   its size, a sprite, unless SMOOTH between pixels mixes its texels; else filled through its
   corners. */
static void draw_source(
    Gx *const g,
    const Place *const where,
    const Source *const p,
    const Vec anchor,
    const Vec a,
    const Vec at,
    const float angle,
    const float scale,
    const int32_t flags
)
{
    const Backend *const b = g->backend;

    if (!b->source(g, p))
        return;

    const Vec top_left = {at.x - anchor.x, at.y - anchor.y};

    if (cmd_upright(where) && angle == 0 && scale == 1 &&
        (!(flags & GFX_DRAW_SMOOTH) || whole(top_left))) {
        b->sprite(g, p, top_left, flags);

        return;
    }

    const Turn t = shape_turn(p->at.w, p->at.h, anchor, a, angle, scale, flags, where);
    b->turned(g, p, &t, flags);
}

static void sprite(
    const int32_t dst,
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
    Place where;

    if (!s || !gx_draw(g, (uint32_t)dst, &c, SPRITE_FLAGS, &where))
        return;

    const Vec anchor = {(float)s->ax, (float)s->ay};

    const Source p = surf_source(&g->surfaces, (uint32_t)s_);

    draw_source(g, &where, &p, anchor, (Vec){x, y}, (Vec){c.x, c.y}, angle, scale, flags);
}

void gfx_draw_sprite(
    const int32_t dst,
    const float x,
    const float y,
    const int32_t s,
    const int32_t flags
)
{
    sprite(dst, s, x, y, 0, 1, flags);
}

void gfx_draw_sprite_turned(
    const int32_t dst,
    const float x,
    const float y,
    const float angle,
    const float scale,
    const int32_t s,
    const int32_t flags
)
{
    sprite(dst, s, x, y, angle, scale, flags);
}

/* x, y the first corner, x2, y2 the third, after the camera. Two triangles, as a GPU draws it. */
void gfx_draw_quad(
    const int32_t dst,
    const QxArray *const corners,
    const int32_t first,
    const int32_t s_,
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
    Place where;

    if (!gx_draw(g, (uint32_t)dst, &c, SPRITE_FLAGS, &where))
        return;

    const Source src = surf_source(&g->surfaces, (uint32_t)s_);
    const Backend *const b = g->backend;
    Corner v[4];

    if (!b->source(g, &src))
        return;

    shape_quad(s->w, s->h, world, flags, &where, v);
    b->quad(g, &src, v, flags);
}

/* x, y the first point, x2, y2 the last, after the camera. The texture repeats along the points,
   their line its top edge, its texels upright: u runs with x from the first point. */
void gfx_draw_strip(
    const int32_t dst,
    const QxArray *const points,
    const int32_t first,
    const int32_t n,
    const int32_t s_,
    const int32_t flags
)
{
    Gx *const g = gx_get();
    const Surface *const s = g ? gx_surface(g, (uint32_t)s_) : nullptr;

    if (!s)
        return;

    if (gx_illegal(n < 2))
        return;

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
    Place where;

    if (!gx_draw(g, (uint32_t)dst, &c, SPRITE_FLAGS, &where))
        return;

    const Source src = surf_source(&g->surfaces, (uint32_t)s_);

    if (!g->backend->source(g, &src))
        return;

    for (int32_t i = 0; i + 1 < n; i++) {
        Corner v[4];
        const Vec a = {p[i].x, p[i].y}, b = {p[i + 1].x, p[i + 1].y};

        shape_segment(s->w, s->h, a, b, p[i].x - p[0].x, flags, &where, v);
        g->backend->poly(g, &src, v, 4, 0, flags | POLY_WRAP);
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

/* A turned map's cells on the target: cell (col, row)'s corners at at + col * across + row * down,
   then lo to hi on; the clip a pixel and a half round, as the scanner's bounds lie half a pixel
   off gfx's and the cells' corners here a rounding off shape_sprite's. */
typedef struct {
    Vec at, across, down, lo, hi;
    float l, r, top, bottom;
} Turned;

static float least(const float a, const float b) { return a < b ? a : b; }

static float most(const float a, const float b) { return a > b ? a : b; }

static Turned turned_of(
    const Place *const where,
    const Vec origin,
    const int32_t w,
    const int32_t h
)
{
    const Vec o = {origin.x - 0.5f, origin.y - 0.5f}, at = cmd_at(where, o);

    const Vec x = cmd_at(where, (Vec){o.x + (float)w, o.y});

    const Vec y = cmd_at(where, (Vec){o.x, o.y + (float)h});

    const Vec across = {x.x - at.x, x.y - at.y}, down = {y.x - at.x, y.y - at.y};

    const Vec far = {across.x + down.x, across.y + down.y};

    const Rect c = where->clip;

    return (Turned){
        .at = at,
        .across = across,
        .down = down,
        .lo =
            {least(least(across.x, down.x), least(far.x, 0)),
             least(least(across.y, down.y), least(far.y, 0))},
        .hi =
            {most(most(across.x, down.x), most(far.x, 0)),
             most(most(across.y, down.y), most(far.y, 0))},
        .l = (float)c.x - 1.5f,
        .r = (float)(c.x + c.w) + 0.5f,
        .top = (float)c.y - 1.5f,
        .bottom = (float)(c.y + c.h) + 0.5f,
    };
}

/* Cell (col, row) may show: its corners' box meets the clip. */
static bool turned_shows(const Turned *const t, const int32_t col, const int32_t row)
{
    const float x = t->at.x + (float)col * t->across.x + (float)row * t->down.x;
    const float y = t->at.y + (float)col * t->across.y + (float)row * t->down.y;

    return x + t->hi.x >= t->l && x + t->lo.x <= t->r && y + t->hi.y >= t->top &&
           y + t->lo.y <= t->bottom;
}

/* The cells of a map columns across, its top left at at (placed), w by h each, that where's clip
   may show. */
static Rect tiles_shown(
    const Place *const where,
    const Vec origin,
    const Vec at,
    const int32_t w,
    const int32_t h,
    const int32_t columns,
    const int32_t rows
)
{
    if (!cmd_upright(where)) {
        Rect cells;
        visible_turned(where, where->clip, origin, w, h, columns, rows, &cells);
        return cells;
    }

    int32_t c0, c1, r0, r1;
    const Rect clip = where->clip;
    visible(cover_pixel(at.x), w, clip.x, clip.x + clip.w, columns, &c0, &c1);
    visible(cover_pixel(at.y), h, clip.y, clip.y + clip.h, rows, &r0, &r1);
    return (Rect){c0, r0, c1 - c0, r1 - r0};
}

/* Every cell of cells a surface, or 0, apart from dst: false, raised. */
static bool tiles_apart(
    Gx *const g,
    const uint32_t dst,
    const int32_t *const map,
    const int32_t columns,
    const Rect cells
)
{
    for (int32_t row = cells.y; row < cells.y + cells.h; row++)
        for (int32_t col = cells.x; col < cells.x + cells.w; col++) {
            const uint32_t cell = (uint32_t)map[row * columns + col];

            if (cell && (!gx_surface(g, cell) || !gx_apart(g, dst, cell)))
                return false;
        }

    return true;
}

/* The visible cells checked before any is drawn, so a refused call draws and logs nothing. */
void gfx_draw_tiles(
    const int32_t dst,
    const float x,
    const float y,
    const QxArray *const map,
    const int32_t columns,
    const int32_t w,
    const int32_t h,
    const int32_t flags
)
{
    Gx *const g = gx_get();

    if (!g || !gx_array(map, sizeof(int32_t), 0))
        return;

    if (gx_illegal(columns < 1 || w < 1 || h < 1))
        return;

    const int32_t rows = (int32_t)map->count / columns;
    const Vec end = {x + (float)(columns * w), y + (float)(rows * h)};

    const int32_t *const cells = map->data;

    Command c = drawn(GFX_DISPLAY_DREW_TILES, (Vec){x, y}, end, 0, flags);

    Place where;

    if (!gx_check(g, (uint32_t)dst, &c, SPRITE_FLAGS, &where))
        return;

    const Vec at = cmd_at(&where, (Vec){x, y});

    const Rect shown = tiles_shown(&where, (Vec){x, y}, at, w, h, columns, rows);

    if (!tiles_apart(g, (uint32_t)dst, cells, columns, shown) ||
        !gx_begin(g, (uint32_t)dst, &c, &where))
        return;

    if (cmd_upright(&where)) {
        for (int32_t row = shown.y; row < shown.y + shown.h; row++)
            for (int32_t col = shown.x; col < shown.x + shown.w; col++) {
                const uint32_t cell = (uint32_t)cells[row * columns + col];

                if (!cell)
                    continue;

                const Vec dx = {(float)(col * w), (float)(row * h)};
                const Source p = surf_source(&g->surfaces, cell);

                draw_source(
                    g,
                    &where,
                    &p,
                    (Vec){},
                    (Vec){x + dx.x, y + dx.y},
                    (Vec){at.x + dx.x, at.y + dx.y},
                    0,
                    1,
                    flags
                );
            }

        return;
    }

    /* each cell's turn the first's moved by whole cells; a source readied once a run of it */
    const Turned shows = turned_of(&where, (Vec){x, y}, w, h);
    const Turn first = shape_turn(w, h, (Vec){}, (Vec){x, y}, 0, 1, flags, &where);
    const Turn step = shape_turn(w, h, (Vec){}, (Vec){x, y}, 0, 1, 0, &where);
    const Vec across = {(float)w * step.across.x, (float)w * step.across.y};
    const Vec down = {(float)h * step.down.x, (float)h * step.down.y};
    const Backend *const b = g->backend;
    uint32_t last = 0;
    Source p = {};

    for (int32_t row = shown.y; row < shown.y + shown.h; row++)
        for (int32_t col = shown.x; col < shown.x + shown.w; col++) {
            const uint32_t cell = (uint32_t)cells[row * columns + col];

            if (!cell || !turned_shows(&shows, col, row))
                continue;

            if (cell != last) {
                p = surf_source(&g->surfaces, cell);
                last = b->source(g, &p) ? cell : 0;

                if (!last)
                    continue;
            }

            const float c = (float)col, r = (float)row;
            Turn t = first;
            t.at = (Vec){first.at.x + c * across.x + r * down.x, first.at.y + c * across.y + r * down.y};
            b->turned(g, &p, &t, flags);
        }
}

/* The style's font and its sheet, apart from dst: null, raised (its sheet freed or dst's, or no
   memory). */
static const Font *style_font(Gx *const g, const uint32_t dst, Source *const sheet)
{
    const Font *const f = font_get(&g->fonts, g->style.font);

    if (!f->sheet) {
        if (font_builtin(&g->fonts, &sheet->root)) {
            sheet->at = (Rect){0, 0, sheet->root.w, sheet->root.h};

            sheet->texture = SOURCE_FONT;
            sheet->version = 1;
            return f;
        }

        gx_fail(ERR_MEMORY);
        return nullptr;
    }

    const Surface *const s = gx_surface(g, f->sheet);

    if (!s || !gx_surface(g, dst) || !gx_apart(g, dst, f->sheet))
        return nullptr;

    *sheet = surf_source(&g->surfaces, f->sheet);
    return f;
}

/* Each glyph a sprite, in colour (SOLID's), or draw.AS_DRAWN its own texels. */
void gfx_draw_text(
    const int32_t dst,
    const float x,
    const float y,
    const QxStr s,
    const int32_t colour
)
{
    Gx *const g = gx_get();
    const bool own = colour == GFX_DRAW_AS_DRAWN;
    Command c = drawn(GFX_DISPLAY_DREW_TEXT, (Vec){x, y}, (Vec){x, y}, own ? 0 : colour, 0);

    Source sheet;
    const Font *const f = g ? style_font(g, (uint32_t)dst, &sheet) : nullptr;
    Place where;

    if (!f || !gx_draw(g, (uint32_t)dst, &c, 0, &where))
        return;

    if (!own)
        g->backend->solid(g, (uint32_t)colour);

    for (uint32_t i = 0; i < s.len; i++) {
        const float dx = (float)((int32_t)i * f->w);
        Rect cell;

        if (!font_glyph(f, (uint8_t)s.ptr[i], &cell))
            continue;

        const Source glyph = {
            sheet.root,
            {sheet.at.x + cell.x, sheet.at.y + cell.y, cell.w, cell.h},
            sheet.texture,
            sheet.version,
        };

        draw_source(g, &where, &glyph, (Vec){}, (Vec){x + dx, y}, (Vec){c.x + dx, c.y}, 0, 1, 0);
    }
}

int32_t gfx_draw_text_width(const QxStr s)
{
    Gx *const g = gx_get();
    return g ? (int32_t)s.len * font_get(&g->fonts, g->style.font)->w : 0;
}
