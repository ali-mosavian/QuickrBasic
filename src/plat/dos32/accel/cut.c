/* A triangle cut to a card's reach, then at a texture's tiles and a wrap's periods (cut.h). */
#include "plat/dos32/accel/cut.h"

enum { S, T, X, Y, STEPS = 4, CHANNEL = 255, BYTE = 8 };

/* A corner as it goes across the screen, linearly: s and t times q. */
typedef struct {
    float x, y, z, q, s, t;
    float colour[4];
} Corner;

typedef struct {
    Corner c[CUT_CORNERS];
    uint32_t n;
} Poly;

typedef struct {
    const CutGrid *g;
    CutPiece *piece;
    void *ctx;
} Cut;

/* The whole number at most f; DOS's C has no libm. */
static int32_t floored(const float f)
{
    const int32_t i = (int32_t)f;
    return (float)i > f ? i - 1 : i;
}

static Corner corner(const AccelVertex *const v)
{
    Corner c = {v->x, v->y, v->z, v->q, v->s * v->q, v->t * v->q, {}};

    for (uint32_t i = 0; i < 4; i++)
        c.colour[i] = (float)(v->colour >> (BYTE * i) & CHANNEL);

    return c;
}

static AccelVertex vertex(const Corner *const c)
{
    AccelVertex v = {c->x, c->y, c->z, c->q, c->s / c->q, c->t / c->q, 0};

    for (uint32_t i = 0; i < 4; i++) {
        const float f = c->colour[i] + 0.5f;
        const uint32_t byte = f <= 0 ? 0 : f >= CHANNEL ? CHANNEL : (uint32_t)f;
        v.colour |= byte << (BYTE * i);
    }

    return v;
}

static Corner between(const Corner *const a, const Corner *const b, const float f)
{
    Corner c = {
        a->x + (b->x - a->x) * f,
        a->y + (b->y - a->y) * f,
        a->z + (b->z - a->z) * f,
        a->q + (b->q - a->q) * f,
        a->s + (b->s - a->s) * f,
        a->t + (b->t - a->t) * f,
        {},
    };

    for (uint32_t i = 0; i < 4; i++)
        c.colour[i] = a->colour[i] + (b->colour[i] - a->colour[i]) * f;

    return c;
}

static float texel(const Corner *const c, const uint32_t axis)
{
    return (axis == S ? c->s : c->t) / c->q;
}

/* How far c is on the kept side of axis's line at: at least at when above, else at most; texels
   along S and T, pixels along X and Y. */
static float side(const Corner *const c, const uint32_t axis, const float at, const bool above)
{
    const float d = axis == S   ? c->s - at * c->q
                    : axis == T ? c->t - at * c->q
                    : axis == X ? c->x - at
                                : c->y - at;
    return above ? d : -d;
}

/* in's part on the kept side of axis's line at into out. */
static void clipped(
    const Poly *const in,
    Poly *const out,
    const uint32_t axis,
    const float at,
    const bool above
)
{
    out->n = 0;

    for (uint32_t i = 0; i < in->n; i++) {
        const Corner *const a = &in->c[i], *const b = &in->c[i + 1 < in->n ? i + 1 : 0];
        const float da = side(a, axis, at, above), db = side(b, axis, at, above);

        if (da >= 0 && out->n < CUT_CORNERS)
            out->c[out->n++] = *a;

        if ((da >= 0) == (db >= 0) || out->n == CUT_CORNERS)
            continue;

        Corner c = between(a, b, da / (da - db));

        /* on the line itself: past a card's reach by a rounding, a corner wraps round */
        if (axis == X)
            c.x = at;
        else if (axis == Y)
            c.y = at;

        out->c[out->n++] = c;
    }
}

/* p's texels along axis moved by -by. */
static void moved(Poly *const p, const uint32_t axis, const float by)
{
    for (uint32_t i = 0; i < p->n; i++) {
        Corner *const c = &p->c[i];

        if (axis == S)
            c->s -= by * c->q;
        else
            c->t -= by * c->q;
    }
}

static void emitted(
    const Cut *const c,
    const Poly *const p,
    const uint32_t column,
    const uint32_t row
)
{
    AccelVertex v[CUT_CORNERS];

    for (uint32_t i = 0; i < p->n; i++)
        v[i] = vertex(&p->c[i]);

    c->piece(c->ctx, column, row, v, p->n);
}

/* p's pieces from step on: s's periods, t's, s's tiles, then t's; column and row, its tile's so
   far. */
static void stepped(
    const Cut *const c,
    const Poly *const p,
    const uint32_t step,
    const uint32_t column,
    const uint32_t row
)
{
    if (p->n < 3)
        return;

    if (step == STEPS) {
        emitted(c, p, column, row);
        return;
    }

    const CutGrid *const g = c->g;
    const uint32_t axis = step % 2;
    const bool tiles = step >= 2, wraps = axis == S ? g->wrap_s : g->wrap_t;

    if (!tiles && !wraps) {
        stepped(c, p, step + 1, column, row);
        return;
    }

    const float origin = tiles ? 0 : (float)(axis == S ? g->wrap.x : g->wrap.y);
    const float size =
        (float)(tiles ? (axis == S ? g->tile_w : g->tile_h) : (axis == S ? g->wrap.w : g->wrap.h));
    const int32_t cells = tiles ? (axis == S ? g->across : g->down) : 0;

    float lo = texel(&p->c[0], axis), hi = lo;

    for (uint32_t i = 1; i < p->n; i++) {
        const float f = texel(&p->c[i], axis);
        lo = f < lo ? f : lo;
        hi = f > hi ? f : hi;
    }

    int32_t first = floored((lo - origin) / size);
    int32_t last = -floored((origin - hi) / size) - 1;

    last = last < first ? first : last;

    /* past the edge tiles, the card holds their edges */
    if (tiles) {
        first = first < 0 ? 0 : first >= cells ? cells - 1 : first;
        last = last < 0 ? 0 : last >= cells ? cells - 1 : last;
    }

    for (int32_t k = first; k <= last; k++) {
        const float at = origin + (float)k * size;

        Poly low, cell;

        low = *p;

        if (k > first)
            clipped(p, &low, axis, at, true);

        cell = low;

        if (k < last)
            clipped(&low, &cell, axis, at + size, false);

        moved(&cell, axis, (float)k * size);

        const uint32_t u = (uint32_t)k;
        stepped(c, &cell, step + 1, tiles && axis == S ? u : column, tiles && axis == T ? u : row);
    }
}

/* Whether v's texels along axis lie from `from` to from + size: in one period, a wrap's none to cut.
   Its corners' bound the triangle's, in perspective too. */
static bool within(const AccelVertex v[3], const uint32_t axis, const float from, const float size)
{
    for (uint32_t i = 0; i < 3; i++) {
        const float f = axis == S ? v[i].s : v[i].t;

        if (f < from || f > from + size)
            return false;
    }

    return true;
}

bool cut_inside(const AccelRect *const band, const AccelVertex *const v, const uint32_t n)
{
    const float x0 = (float)band->x, y0 = (float)band->y;
    const float x1 = (float)(band->x + band->w), y1 = (float)(band->y + band->h);

    for (uint32_t i = 0; i < n && band->w; i++)
        if (v[i].x < x0 || v[i].x > x1 || v[i].y < y0 || v[i].y > y1)
            return false;

    return true;
}

/* Triangle v's part within g's band, cut by x and y, on as triangles. */
static void banded(
    const CutGrid *const g,
    const AccelVertex v[3],
    CutPiece *const piece,
    void *const ctx
)
{
    const AccelRect *const b = &g->band;
    const float from[] = {(float)b->x, (float)b->y};
    const float to[] = {(float)(b->x + b->w), (float)(b->y + b->h)};
    Poly p = {{corner(&v[0]), corner(&v[1]), corner(&v[2])}, 3}, q;

    for (uint32_t axis = X; axis <= Y; axis++) {
        clipped(&p, &q, axis, from[axis - X], true);
        clipped(&q, &p, axis, to[axis - X], false);
    }

    /* its corners on the band's edges, a rounding past them is not cut again */
    CutGrid within = *g;

    within.band.w = 0;

    for (uint32_t i = 1; i + 1 < p.n; i++) {
        const AccelVertex t[3] = {vertex(&p.c[0]), vertex(&p.c[i]), vertex(&p.c[i + 1])};

        cut_triangle(&within, t, piece, ctx);
    }
}

void cut_triangle(
    const CutGrid *const g,
    const AccelVertex v[3],
    CutPiece *const piece,
    void *const ctx
)
{
    if (!cut_inside(&g->band, v, 3)) {
        banded(g, v, piece, ctx);
        return;
    }

    const bool wrap_s = g->wrap_s && !within(v, S, (float)g->wrap.x, (float)g->wrap.w);
    const bool wrap_t = g->wrap_t && !within(v, T, (float)g->wrap.y, (float)g->wrap.h);

    if (!wrap_s && !wrap_t && g->across == 1 && g->down == 1) {
        piece(ctx, 0, 0, v, 3);
        return;
    }

    CutGrid k = *g;

    k.wrap_s = wrap_s;
    k.wrap_t = wrap_t;

    const Cut c = {&k, piece, ctx};

    const Poly p = {{corner(&v[0]), corner(&v[1]), corner(&v[2])}, 3};

    stepped(&c, &p, 0, 0, 0);
}
