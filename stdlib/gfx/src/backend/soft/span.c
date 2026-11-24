/* Spans (span.h). */
#include "backend/soft/span.h"

uint8_t *span_at(const SoftTarget *const t, const int32_t x, const int32_t y)
{
    return t->px.p + y * t->px.pitch + x * px_bytes(t->px.format);
}

bool span_fused(const SoftTarget *const t, const Pixels *const s)
{
    return !t->ink.effects && s->format == t->px.format;
}

static bool blends(const Ink *const k) { return k->blend || k->mode; }

/* n pixels from x, y through the call's ink: s's texels along the walk, inside s, or colour when
   s is null (drawn even when 0). */
static void inked(
    const SoftTarget *const t,
    const int32_t x,
    const int32_t y,
    const int32_t n,
    const Pixels *const s,
    Steps st,
    const uint32_t colour
)
{
    uint32_t texels[FILL_CHUNK];
    Ink k = t->ink;
    k.keyed = k.keyed && s;
    k.source = s ? s->format : k.source;
    const uint32_t bytes = px_bytes(t->px.format);

    for (int32_t done = 0; done < n; done += FILL_CHUNK) {
        const int32_t m = rect_min(n - done, FILL_CHUNK);

        if (s) {
            fill_formats[s->format]->fetch(texels, s, st, m);
            st = steps_skip(st, m);
        } else
            for (int32_t i = 0; i < m; i++)
                texels[i] = colour;

        t->fill->ink(span_at(t, x, y) + done * bytes, texels, m, &k);
    }
}

void span_flat(
    const SoftTarget *const t,
    const int32_t y,
    const int32_t x0,
    const int32_t x1,
    const uint32_t colour
)
{
    const int32_t from = rect_max(x0, t->clip.x), to = rect_min(x1, t->clip.x + t->clip.w);

    if (y >= t->clip.y && y < t->clip.y + t->clip.h && from < to)
        span_fill(t, y, from, to, colour);
}

void span_fill(
    const SoftTarget *const t,
    const int32_t y,
    const int32_t x0,
    const int32_t x1,
    const uint32_t colour
)
{
    if (x0 >= x1)
        return;

    if (blends(&t->ink))
        inked(t, x0, y, x1 - x0, nullptr, (Steps){}, colour);
    else
        t->fill->span(span_at(t, x0, y), x1 - x0, t->fill->paint(colour, &t->ink));
}

/* n pixels a pixel at a time: each texel held inside, keyed, shown over the depth there, drawn,
   its depth set. */
static void each(
    const Textured *const x,
    const int32_t px,
    const int32_t y,
    const int32_t n,
    Steps st,
    int32_t z,
    const int32_t dz
)
{
    const SoftTarget *const t = x->t;
    const Pixels *const s = x->s;
    const uint32_t bytes = px_bytes(t->px.format);
    const bool plain = span_fused(t, s);
    uint16_t *const d = t->depth.p ? t->depth.p + y * t->depth.row + px : nullptr;
    uint8_t *const out = span_at(t, px, y);
    Ink k = t->ink;
    k.keyed = false;
    k.source = s->format;

    for (int32_t i = 0; i < n; i++, z = depth_on(z, dz, 1), steps_on(&st)) {
        const uint32_t texel = steps_texel(s, &st);
        const uint16_t here = depth_of(z);

        if (d && x->test && !depth_shows(d[i], here))
            continue;

        if (!x->opaque && px_key(s->format, texel))
            continue;

        if (plain)
            px_store(out + i * bytes, texel, bytes);
        else
            t->fill->ink(out + i * bytes, &texel, 1, &k);

        if (d && x->set)
            d[i] = here;
    }
}

void span_texels(
    const Textured *const x,
    const int32_t px,
    const int32_t y,
    const int32_t n,
    const Steps st,
    const int32_t z,
    const int32_t dz
)
{
    const SoftTarget *const t = x->t;

    if (t->depth.p || !steps_inside(&st, x->s, n))
        each(x, px, y, n, st, z, dz);
    else if (span_fused(t, x->s))
        t->fill->step[x->opaque ? FILL_OPAQUE : 0](span_at(t, px, y), x->s, st, n);
    else
        inked(t, px, y, n, x->s, st, 0);
}

typedef struct {
    int32_t u, v;
} UV;

/* PDIV: 65536 u/z and 65536 v/z rounded, and half a texel on this side of the divide */
static UV pdiv(const double u, const double v, const double z)
{
    const double zf = 65536.0 / z;

    return (UV){
        (int32_t)((uint32_t)scan_fistp(zf * u) + 32768u),
        (int32_t)((uint32_t)scan_fistp(zf * v) + 32768u),
    };
}

/* PMKSTEP: a run's step a pixel, its length SCAN_SUBDIVP */
static int32_t step_of(const int32_t from, const int32_t to)
{
    return (int32_t)((uint32_t)to - (uint32_t)from) >> SCAN_SUBDIVS;
}

void span_persp(const Textured *const x, const ScanSpan *const sp)
{
    const int32_t n = sp->x1 - sp->x0, wrap = x->wrap ? x->s->w << 16 : 0;
    double u = sp->pu, v = sp->pv, z = sp->pz;
    UV p = pdiv(u, v, z);

    for (int32_t done = 0; done < n; done += SCAN_SUBDIVP) {
        const int32_t m = rect_min(n - done, SCAN_SUBDIVP);

        /* one whole run on, a short tail or not */
        u += sp->dpun;
        v += sp->dpvn;
        z += sp->dpzn;

        const UV l = pdiv(u, v, z);
        const Steps st = steps_from(p.u, p.v, step_of(p.u, l.u), step_of(p.v, l.v), wrap);

        span_texels(x, sp->x0 + done, sp->y, m, st, depth_on(sp->z, sp->dz, done), sp->dz);
        p = l;
    }
}
