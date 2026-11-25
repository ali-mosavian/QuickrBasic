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

/* A span's filler, picked once: the fused one when the ink has nothing to do, else the inked one
   for the source and the ink's variant. */
typedef struct {
    FillStep *step;
    FillInked *inked;
} Filler;

static Filler filler_of(const Textured *const x)
{
    const SoftTarget *const t = x->t;
    const Pixels *const s = x->s;

    if (span_fused(t, s))
        return (Filler){.step = t->fill->step[x->opaque ? FILL_OPAQUE : 0]};

    return (Filler){.inked =
                        t->fill->inked[s->format][fill_variant(&t->ink, t->px.format, s->format)]};
}

/* n pixels into d by f along st, which does not wrap. */
static void drawn(
    const Textured *const x,
    const Filler f,
    uint8_t *const d,
    const int32_t n,
    const Steps st
)
{
    if (f.step)
        f.step(d, x->s, st, n);
    else
        f.inked(d, x->s, st, n, &x->t->ink);
}

/* n pixels from x, y by the span's filler. A wrapping walk is cut where u wraps, so none wraps. */
[[gnu::always_inline]]
static inline void filled(
    const Textured *const x,
    const int32_t px,
    const int32_t y,
    const int32_t n,
    Steps st
)
{
    const Filler f = filler_of(x);

    for (int32_t done = 0; done < n;) {
        const int32_t m = st.wrap ? steps_unwrapped(&st, n - done) : n - done;

        drawn(x, f, span_at(x->t, px + done, y), m, (Steps){st.u, st.v, st.du, st.dv, 0});

        done += m;
        st.u = st.wrap ? steps_wrap(st.u + m * st.du, st.wrap) : st.u;
        st.v += m * st.dv;
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
        t->fill->tint(span_at(t, x0, y), x1 - x0, colour, &t->ink, t->tints);
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

/* The texel at u, v, whole texels, held inside s; u wrapped when it wraps. */
static uint32_t texel_at(const Pixels *const s, const int32_t u, const int32_t v, const bool wrap)
{
    const uint32_t bytes = px_bytes(s->format);
    const int32_t across = wrap ? steps_wrap(u, s->w) : steps_held(u, s->w);

    return px_load(s->p + steps_held(v, s->h) * s->pitch + (uint32_t)across * bytes, bytes);
}

/* A colour's channels, 0 to 1, and its coverage: 1 drawn, 0 keyed. */
typedef struct {
    float r, g, b, a;
} Mix;

static Mix mix_of(const uint32_t argb, const bool drawn)
{
    const float n = drawn ? 1.0f / 255 : 0;

    return (Mix){(float)(argb >> 16 & 0xFF) * n,
                 (float)(argb >> 8 & 0xFF) * n,
                 (float)(argb & 0xFF) * n,
                 drawn ? 1.0f : 0};
}

/* GLSL's mix: a to b by f. */
static Mix mix(const Mix a, const Mix b, const float f)
{
    const float e = 1 - f;
    return (Mix){a.r * e + b.r * f, a.g * e + b.g * f, a.b * e + b.b * f, a.a * e + b.a * f};
}

static uint32_t channel(const float c) { return (uint32_t)(c * 255 + 0.5f); }

/* The four texels round the walk's place through k but its blend, mixed as the GPU's shader mixes
   them: the place half a texel back, its fractions the weights; a keyed texel adds nothing. Into
   *argb, the mix's colour divided by its coverage: false, under half covered, not drawn. */
static bool mixed(
    const Textured *const x,
    const Steps *const st,
    const Ink *const k,
    uint32_t *argb
)
{
    const int32_t pu = (int32_t)((uint32_t)st->u - 0x8000u),
                  pv = (int32_t)((uint32_t)st->v - 0x8000u);
    const int32_t u = pu >> 16, v = pv >> 16;
    const float fu = (float)(pu & 0xFFFF) / 65536, fv = (float)(pv & 0xFFFF) / 65536;
    const uint32_t texels[] = {
        texel_at(x->s, u, v, x->wrap),
        texel_at(x->s, u + 1, v, x->wrap),
        texel_at(x->s, u, v + 1, x->wrap),
        texel_at(x->s, u + 1, v + 1, x->wrap),
    };

    uint32_t c[4];
    bool drawn[4];

    fill_argb(c, drawn, texels, 4, k);
    const Mix top = mix(mix_of(c[0], drawn[0]), mix_of(c[1], drawn[1]), fu);
    const Mix under = mix(mix_of(c[2], drawn[2]), mix_of(c[3], drawn[3]), fu);
    const Mix m = mix(top, under, fv);

    if (m.a < 0.5f)
        return false;

    *argb = PX_OPAQUE | channel(m.r / m.a) << 16 | channel(m.g / m.a) << 8 | channel(m.b / m.a);
    return true;
}

/* n pixels SMOOTH's way, a pixel at a time: the mix shown over the depth there, drawn, its depth
   set. A SOLID mix is its colour, where covered. */
static void smooth(
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
    const uint32_t bytes = px_bytes(t->px.format);
    uint16_t *const d = t->depth.p ? t->depth.p + y * t->depth.row + px : nullptr;
    uint8_t *const out = span_at(t, px, y);
    Ink k = t->ink, drawn = t->ink;
    k.source = x->s->format;
    drawn.keyed = false;

    if (!k.solid) {
        drawn.source = PX_32;
        drawn.shade = PX_WHITE;
    }

    for (int32_t i = 0; i < n; i++, z = depth_on(z, dz, 1), steps_on(&st)) {
        const uint16_t here = depth_of(z);
        uint32_t argb;

        if (d && x->test && !depth_shows(d[i], here))
            continue;

        if (!mixed(x, &st, &k, &argb))
            continue;

        t->fill->ink(out + i * bytes, &argb, 1, &drawn);

        if (d && x->set)
            d[i] = here;
    }
}

/* st i pixels on; u wrapped by a divide, as i may be many widths. */
static Steps steps_ahead(Steps st, const int32_t i)
{
    st.u += i * st.du;
    st.v += i * st.dv;
    st.u = st.wrap ? (st.u % st.wrap + st.wrap) % st.wrap : st.u;
    return st;
}

/* The first of n pixels where c, stepped by dc a pixel, crosses limit (all 16.16): n, none. */
static int32_t crossing(const int64_t c, const int64_t dc, const int64_t limit, const int32_t n)
{
    const int64_t i = c < limit && dc > 0    ? (limit - c + dc - 1) / dc
                      : c >= limit && dc < 0 ? (c - limit) / -dc + 1
                                             : n;

    return i < n ? (int32_t)i : n;
}

/* c held inside [0, size) as steps_texel holds a texel: past an end, that end's texel, its step
   0. */
static void held(int32_t *const c, int32_t *const dc, const int32_t size)
{
    if (*c < 0 || (int64_t)*c >= (int64_t)size << 16) {
        *c = *c < 0 ? 0 : (size - 1) << 16;
        *dc = 0;
    }
}

/* n pixels from px, y along st, each texel held inside x's picture as each holds it, by the
   filler: cut where u or v crosses an edge, as it steps straight, a coordinate held in a piece
   past its edge. */
static void pieces(
    const Textured *const x,
    const int32_t px,
    const int32_t y,
    const int32_t n,
    const Steps st
)
{
    const Pixels *const s = x->s;
    const int64_t w = (int64_t)s->w << 16, h = (int64_t)s->h << 16;
    int32_t at[] = {
        0,
        n,
        st.wrap ? n : crossing(st.u, st.du, 0, n),
        st.wrap ? n : crossing(st.u, st.du, w, n),
        crossing(st.v, st.dv, 0, n),
        crossing(st.v, st.dv, h, n),
    };

    const int32_t cuts = sizeof at / sizeof *at;

    for (int32_t i = 1; i < cuts; i++)
        for (int32_t j = i; j > 0 && at[j] < at[j - 1]; j--) {
            const int32_t a = at[j];
            at[j] = at[j - 1];
            at[j - 1] = a;
        }

    for (int32_t i = 0; i + 1 < cuts; i++)
        if (at[i] < at[i + 1]) {
            Steps p = steps_ahead(st, at[i]);

            if (!st.wrap)
                held(&p.u, &p.du, s->w);

            held(&p.v, &p.dv, s->h);
            filled(x, px + at[i], y, at[i + 1] - at[i], p);
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

    if (x->smooth)
        smooth(x, px, y, n, st, z, dz);
    else if (t->depth.p)
        each(x, px, y, n, st, z, dz);
    else if (steps_inside(&st, x->s, n))
        filled(x, px, y, n, st);
    else
        pieces(x, px, y, n, st);
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
    const SoftTarget *const t = x->t;
    const int32_t n = sp->x1 - sp->x0, wrap = x->wrap ? x->s->w << 16 : 0;
    const bool plain = !x->smooth && !t->depth.p && !wrap; /* runs the filler takes as they are */
    const Filler f = filler_of(x);
    uint8_t *const row = span_at(t, sp->x0, sp->y);
    const int32_t bytes = (int32_t)px_bytes(t->px.format);
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

        if (plain && steps_inside(&st, x->s, m))
            drawn(x, f, row + done * bytes, m, st);
        else
            span_texels(x, sp->x0 + done, sp->y, m, st, depth_on(sp->z, sp->dz, done), sp->dz);

        p = l;
    }
}
