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

/* n pixels from x, y by the span's filler. A wrapping walk is cut where u wraps, so none wraps,
   unless its mask wraps it (steps_masked). */
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
    const bool masked = steps_masked(&st, x->s->w, n);

    for (int32_t done = 0; done < n;) {
        const int32_t m = st.wrap && !masked ? steps_unwrapped(&st, n - done) : n - done;
        const Steps at = {st.u, st.v, st.du, st.dv, masked ? st.wrap : 0};

        drawn(x, f, span_at(x->t, px + done, y), m, at);

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

/* The texel at u, v, whole texels, held inside s; u wrapped when it wraps. */
static uint32_t texel_at(const Pixels *const s, const int32_t u, const int32_t v, const bool wrap)
{
    const uint32_t bytes = px_bytes(s->format);
    const int32_t across = wrap ? steps_wrap(u, s->w) : steps_held(u, s->w);

    return px_load(s->p + steps_held(v, s->h) * s->pitch + (uint32_t)across * bytes, bytes);
}

/* The four texels from u, v to u + 1, v + 1, whole texels, of s, g's: read from one place when
   all are inside, else each held or wrapped. */
[[gnu::always_inline]]
static inline void corners(
    uint32_t *const out,
    const PxFormat g,
    const Pixels *const s,
    const int32_t u,
    const int32_t v,
    const bool wrap
)
{
    const uint32_t bytes = px_bytes(g);

    if ((uint32_t)u < (uint32_t)(s->w - 1) && (uint32_t)v < (uint32_t)(s->h - 1)) {
        const uint8_t *const p = s->p + (uint32_t)v * s->pitch + (uint32_t)u * bytes;

        out[0] = px_load(p, bytes);
        out[1] = px_load(p + bytes, bytes);
        out[2] = px_load(p + s->pitch, bytes);
        out[3] = px_load(p + s->pitch + bytes, bytes);
        return;
    }

    out[0] = texel_at(s, u, v, wrap);
    out[1] = texel_at(s, u + 1, v, wrap);
    out[2] = texel_at(s, u, v + 1, wrap);
    out[3] = texel_at(s, u + 1, v + 1, wrap);
}

/* A colour's channels, 0 to 1, and its coverage: 1 drawn, 0 keyed. */
typedef struct {
    float r, g, b, a;
} Mix;

/* A channel, 0 to 1: c times a 255th, made once. */
#define UNIT(c) (float)(c) * (1.0f / 255),
#define UNITS4(c) UNIT(c) UNIT(c + 1) UNIT(c + 2) UNIT(c + 3)
#define UNITS16(c) UNITS4(c) UNITS4(c + 4) UNITS4(c + 8) UNITS4(c + 12)
#define UNITS64(c) UNITS16(c) UNITS16(c + 16) UNITS16(c + 32) UNITS16(c + 48)
static const float units[256] = {UNITS64(0) UNITS64(64) UNITS64(128) UNITS64(192)};

[[gnu::always_inline]]
static inline Mix mix_of(const uint32_t argb, const bool drawn)
{
    if (!drawn)
        return (Mix){0, 0, 0, 0};

    return (Mix){units[argb >> 16 & 0xFF], units[argb >> 8 & 0xFF], units[argb & 0xFF], 1};
}

/* GLSL's mix: a to b by f. */
[[gnu::always_inline]]
static inline Mix mix(const Mix a, const Mix b, const float f)
{
    const float e = 1 - f;
    return (Mix){a.r * e + b.r * f, a.g * e + b.g * f, a.b * e + b.b * f, a.a * e + b.a * f};
}

/* Four texels' ARGB c, those drawn, mixed by fu across and fv down as the GPU's shader mixes
   them: a keyed texel adds nothing. */
[[gnu::always_inline]]
static inline Mix mixed(
    const uint32_t *const c,
    const bool *const drawn,
    const float fu,
    const float fv
)
{
    const Mix top = mix(mix_of(c[0], drawn[0]), mix_of(c[1], drawn[1]), fu);
    const Mix under = mix(mix_of(c[2], drawn[2]), mix_of(c[3], drawn[3]), fu);
    return mix(top, under, fv);
}

static uint32_t channel(const float c) { return (uint32_t)(c * 255 + 0.5f); }

static uint32_t opaque(const float r, const float g, const float b)
{
    return PX_OPAQUE | channel(r) << 16 | channel(g) << 8 | channel(b);
}

/* mixed's colour divided by its coverage into *argb: false, under half covered, not drawn. */
static bool partly(
    const uint32_t *const c,
    const bool *const drawn,
    const float fu,
    const float fv,
    uint32_t *const argb
)
{
    const Mix m = mixed(c, drawn, fu, fv);

    if (m.a < 0.5f)
        return false;

    *argb = opaque(m.r / m.a, m.g / m.a, m.b / m.a);
    return true;
}

/* partly's, four drawn: they cover it all, exactly 1, so none is divided. */
[[gnu::always_inline]]
static inline bool covered(
    const uint32_t *const c,
    const bool *const drawn,
    const float fu,
    const float fv,
    uint32_t *const argb
)
{
    if (!(drawn[0] & drawn[1] & drawn[2] & drawn[3]))
        return partly(c, drawn, fu, fv, argb);

    const Mix m = mixed(c, (const bool[]){true, true, true, true}, fu, fv);

    *argb = opaque(m.r, m.g, m.b);
    return true;
}

/* The first of m pixels from i not shown: m, none. */
static int32_t shown_to(const bool *const shown, int32_t i, const int32_t m)
{
    while (i < m && shown[i])
        i++;

    return i;
}

enum { SMOOTH_CHUNK = 64 };

/* n pixels SMOOTH's way, g's texels, SMOOTH_CHUNK at a time: the four texels round each pixel's
   place through k but its blend, mixed, the place half a texel back and its fractions the
   weights; the mixes shown over the depth there drawn a run at a time, their depth set. A SOLID
   mix is its colour, where covered. */
[[gnu::always_inline]]
static inline void smooth_of(
    const PxFormat g,
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
    Ink k = t->ink, ink = t->ink;
    k.source = g;
    ink.keyed = false;

    if (!k.solid) {
        ink.source = PX_32;
        ink.shade = PX_WHITE;
    }

    for (int32_t at = 0; at < n; at += SMOOTH_CHUNK) {
        const int32_t m = n - at < SMOOTH_CHUNK ? n - at : SMOOTH_CHUNK;
        uint32_t texels[4 * SMOOTH_CHUNK], c[4 * SMOOTH_CHUNK], argb[SMOOTH_CHUNK];
        bool drawn[4 * SMOOTH_CHUNK], shown[SMOOTH_CHUNK];
        float fu[SMOOTH_CHUNK], fv[SMOOTH_CHUNK];

        for (int32_t i = 0; i < m; i++, steps_on(&st)) {
            const int32_t pu = (int32_t)((uint32_t)st.u - 0x8000u),
                          pv = (int32_t)((uint32_t)st.v - 0x8000u);

            fu[i] = (float)(pu & 0xFFFF) / 65536;
            fv[i] = (float)(pv & 0xFFFF) / 65536;
            corners(texels + 4 * i, g, x->s, pu >> 16, pv >> 16, x->wrap);
        }

        fill_argb(c, drawn, texels, 4 * m, &k);

        for (int32_t i = 0; i < m; i++, z = depth_on(z, dz, 1)) {
            const uint16_t here = depth_of(z);
            uint16_t *const there = d ? d + at + i : nullptr;

            shown[i] = (!there || !x->test || depth_shows(*there, here)) &&
                       covered(c + 4 * i, drawn + 4 * i, fu[i], fv[i], argb + i);

            if (shown[i] && there && x->set)
                *there = here;
        }

        for (int32_t i = 0; i < m;) {
            const int32_t end = shown_to(shown, i, m);

            if (end > i)
                t->fill->ink(out + (at + i) * (int32_t)bytes, argb + i, end - i, &ink);

            i = end + 1;
        }
    }
}

/* smooth_of, the source's body picked once. */
static void smooth(
    const Textured *const x,
    const int32_t px,
    const int32_t y,
    const int32_t n,
    const Steps st,
    const int32_t z,
    const int32_t dz
)
{
    switch (x->s->format) {
    case PX_8:
        smooth_of(PX_8, x, px, y, n, st, z, dz);
        break;

    case PX_15:
        smooth_of(PX_15, x, px, y, n, st, z, dz);
        break;

    case PX_16:
        smooth_of(PX_16, x, px, y, n, st, z, dz);
        break;

    default:
        smooth_of(PX_32, x, px, y, n, st, z, dz);
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

/* The first of n pixels where c, stepped by dc a pixel, crosses limit (all 16.16): n, none. A
   limit is at most a side's, 2^30, so each distance fits 32 bits, unsigned: no divide is 64
   bits'. */
static int32_t crossing(const int32_t c, const int32_t dc, const int32_t limit, const int32_t n)
{
    const bool up = c < limit && dc > 0, down = c >= limit && dc < 0;
    const uint32_t far = up ? (uint32_t)limit - (uint32_t)c - 1 : (uint32_t)c - (uint32_t)limit;
    const uint32_t i = up || down ? far / (up ? (uint32_t)dc : 0u - (uint32_t)dc) + 1 : (uint32_t)n;

    return i < (uint32_t)n ? (int32_t)i : n;
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
    const int32_t w = s->w << 16, h = s->h << 16;
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

/* n pixels from px, y along st by the filler, held inside x's picture. */
static void run(
    const Textured *const x,
    const int32_t px,
    const int32_t y,
    const int32_t n,
    const Steps st
)
{
    if (steps_inside(&st, x->s, n))
        filled(x, px, y, n, st);
    else
        pieces(x, px, y, n, st);
}

/* Into out, whether each of n texels along st, g's, is drawn, not keyed: read by the walk when
   inside s. */
[[gnu::always_inline]]
static inline void kept_of(
    const PxFormat g,
    bool *const out,
    const Pixels *const s,
    Steps st,
    const int32_t n
)
{
    if (!st.wrap && steps_inside(&st, s, n)) {
        const StepsBy by = steps_by(s, st, px_bytes(g));
        StepsRead r = steps_read(s, st, &by, px_bytes(g));

        for (int32_t i = 0; i < n; i++)
            out[i] = !px_key(g, steps_read_next(&r, px_bytes(g)));

        return;
    }

    for (int32_t i = 0; i < n; i++, steps_on(&st))
        out[i] = !px_key(g, steps_texel(s, &st));
}

/* kept_of, the source's body picked once. */
static void kept(bool *const out, const Pixels *const s, const Steps st, const int32_t n)
{
    switch (s->format) {
    case PX_8:
        kept_of(PX_8, out, s, st, n);
        break;

    case PX_15:
        kept_of(PX_15, out, s, st, n);
        break;

    case PX_16:
        kept_of(PX_16, out, s, st, n);
        break;

    default:
        kept_of(PX_32, out, s, st, n);
    }
}

enum { DEEP_CHUNK = 64 };

/* n pixels over the depth from z by dz, DEEP_CHUNK at a time: those in front, and drawn where a
   keyed texel sets none, have their depth set, each run of them drawn as span_texels draws one. */
static void deep(
    const Textured *const x,
    const int32_t px,
    const int32_t y,
    const int32_t n,
    const Steps st,
    int32_t z,
    const int32_t dz
)
{
    const SoftTarget *const t = x->t;
    uint16_t *const d = t->depth.p + y * t->depth.row + px;
    const bool keyed = x->set && !x->opaque; /* a run's filler keys its texels; their depth, here */

    for (int32_t at = 0; at < n; at += DEEP_CHUNK) {
        const int32_t m = n - at < DEEP_CHUNK ? n - at : DEEP_CHUNK;
        const Steps from = steps_ahead(st, at);
        bool shown[DEEP_CHUNK];

        if (keyed)
            kept(shown, x->s, from, m);

        for (int32_t i = 0; i < m; i++, z = depth_on(z, dz, 1)) {
            const uint16_t here = depth_of(z);
            uint16_t *const there = d + at + i;

            shown[i] = (!keyed || shown[i]) && (!x->test || depth_shows(*there, here));

            if (shown[i] && x->set)
                *there = here;
        }

        for (int32_t i = 0; i < m;) {
            const int32_t end = shown_to(shown, i, m);

            if (end > i)
                run(x, px + at + i, y, end - i, steps_ahead(from, i));

            i = end + 1;
        }
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
    if (x->smooth)
        smooth(x, px, y, n, st, z, dz);
    else if (x->t->depth.p)
        deep(x, px, y, n, st, z, dz);
    else
        run(x, px, y, n, st);
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
