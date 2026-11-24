/* The perspective span filler (persp.h). */
#include "soft/persp.h"

typedef struct {
    int32_t u, v;
} UV;

/* PDIV: 65536 u/z and 65536 v/z rounded, and half a texel on this side of the divide */
static UV pdiv(const double u, const double v, const double z)
{
    const double zf = 65536.0 / z;

    return (UV){
        (int32_t)((uint32_t)rs_fistp(zf * u) + 32768u),
        (int32_t)((uint32_t)rs_fistp(zf * v) + 32768u),
    };
}

static int32_t held(const int32_t x, const int32_t lo, const int32_t hi)
{
    return x < lo ? lo : x > hi ? hi : x;
}

/* n pixels from pixel first, walking from u, v by du, dv; their depth from z by dz */
static void walk(
    const PerspDraw *const k,
    const int32_t first,
    const int32_t n,
    int32_t u,
    int32_t v,
    const int32_t du,
    const int32_t dv,
    const RsSpan *const sp
)
{
    const uint32_t bytes = px_bytes(k->s.format);
    const int32_t w = (int32_t)k->s.w, h = (int32_t)k->s.h;
    int32_t z = depth_on(sp->z, sp->dz, first);

    for (int32_t i = first; i < first + n; i++, z = depth_on(z, sp->dz, 1)) {
        int32_t iu = u >> 16;

        if (k->wrap) {
            iu %= w;
            iu += iu < 0 ? w : 0;
        } else
            iu = held(iu, 0, w - 1);

        const int32_t iv = held(v >> 16, 0, h - 1);
        const uint32_t texel = px_load(k->s.p + iv * k->s.pitch + iu * bytes, bytes);
        const uint16_t here = depth_of(z);
        uint8_t *const out = k->d + i * k->bytes;

        u = (int32_t)((uint32_t)u + (uint32_t)du);
        v = (int32_t)((uint32_t)v + (uint32_t)dv);

        if (k->depth && k->test && !depth_shows(k->depth[i], here))
            continue;

        if (!k->opaque && px_key(k->s.format, texel))
            continue;

        if (k->ink)
            k->put(out, &texel, 1, k->ink);
        else
            px_store(out, texel, bytes);

        if (k->depth && k->set)
            k->depth[i] = here;
    }
}

/* PMKSTEP: a run's step a pixel, its length RS_SUBDIVP */
static int32_t step_of(const int32_t from, const int32_t to)
{
    return (int32_t)((uint32_t)to - (uint32_t)from) >> RS_SUBDIVS;
}

void persp_span(const PerspDraw *const k, const RsSpan *const sp)
{
    const int32_t n = sp->x1 - sp->x0;
    double u = sp->pu, v = sp->pv, z = sp->pz;

    if (n < 1)
        return;

    UV p = pdiv(u, v, z);

    if (k->whole) {
        /* ASTEP, AMKSTEP: one run, the step an idiv */
        u += (double)sp->dpu * n;
        v += (double)sp->dpv * n;
        z += (double)sp->dpz * n;

        const UV l = pdiv(u, v, z);
        walk(
            k,
            0,
            n,
            p.u,
            p.v,
            (int32_t)((uint32_t)l.u - (uint32_t)p.u) / n,
            (int32_t)((uint32_t)l.v - (uint32_t)p.v) / n,
            sp
        );
        return;
    }

    for (int32_t done = 0; done < n; done += RS_SUBDIVP) {
        const int32_t m = n - done < RS_SUBDIVP ? n - done : RS_SUBDIVP;

        /* one whole run on, a short tail or not */
        u += sp->dpun;
        v += sp->dpvn;
        z += sp->dpzn;

        const UV l = pdiv(u, v, z);
        walk(k, done, m, p.u, p.v, step_of(p.u, l.u), step_of(p.v, l.v), sp);
        p = l;
    }
}
