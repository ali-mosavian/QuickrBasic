/* A triangle and its state as an SST card takes them (setup.h). */
#include "plat/dos32/drivers/3dfx/setup.h"
#include "plat/dos32/drivers/3dfx/regs.h"
#include "lib/reciprocal.h"

enum { CHANNEL = 255, DEPTH = 65535, SIDE = 256, LEVEL = 4, SIGN = 1u << 31 };

enum : uint32_t {
    COLOUR = 1u << TDFX_R | 1u << TDFX_G | 1u << TDFX_B,
    TEXEL = 1u << TDFX_S | 1u << TDFX_T | 1u << TDFX_W,
    CORNERS = (1u << TDFX_CORNERS) - 1,
    RGB = 0xFFFFFF,
};

/* Alpha's share as a multiply, a divide being 39 cycles on a Pentium: the float is above 1/255, so a
   whole alpha stays whole as the card truncates it. */
static const float FADE = 1.0f / CHANNEL;

static uint32_t log2_of(uint32_t n)
{
    uint32_t l = 0;

    while (n > 1) {
        n >>= 1;
        l++;
    }

    return l;
}

/* A float's bits, as the float registers take them. */
static uint32_t bits(const float v)
{
    const union {
        float f;
        uint32_t bits;
    } u = {.f = v};

    return u.bits;
}

/* The float of bits b. */
static float of_bits(const uint32_t b)
{
    float f;

    __builtin_memcpy(&f, &b, sizeof f);
    return f;
}

enum : uint32_t { ONE = 0x3F800000 }; /* 1.0f's bits */

/* The bits of float b times 2 to the k, by the FPU. Apart, so that scaled's b is not loaded as a
   float for it. */
[[gnu::noinline]]
static uint32_t multiplied(const uint32_t b, const uint32_t k)
{
    return bits(of_bits(b) * (float)(int32_t)(1u << k));
}

/* The bits of float b times 2 to the k, by its exponent: off a 486's FPU, whose load, multiply
   and store cost 20 cycles a texel. A zero is itself, half a sprite's texels; a denormal or a
   result out of range is the FPU's. */
static uint32_t scaled(const uint32_t b, const uint32_t k)
{
    const uint32_t e = b >> 23 & 0xFF;

    if (e - 1 < 0xFEu - k)
        return b + (k << 23);

    return b << 1 ? multiplied(b, k) : b;
}

/* The colour unless the texel alone or no colour written; alpha for a blend or the key's test. */
uint32_t tdfx_params(const AccelState *const s, const bool texel)
{
    const bool alone = texel && s->source == ACCEL_TEXEL && s->flags & ACCEL_REPLACE;
    const bool alpha = s->blend == ACCEL_MIX || s->blend == ACCEL_ADD || s->flags & ACCEL_KEY;

    return (alone || s->blend == ACCEL_KEEP ? 0 : COLOUR) | (alone || !alpha ? 0 : 1u << TDFX_A) |
           (s->depth ? 1u << TDFX_Z : 0) | (texel ? TEXEL : 0);
}

/* W's scale for v's n corners: the largest q made 1, a 2D draw's q of 1 left undivided. A q is
   above 0, its bits ordered as it is: compared as integers, not by the FPU's slow compares. */
static float w_scale(const AccelVertex *const v, const uint32_t n)
{
    uint32_t most = 0;

    for (uint32_t i = 1; i < n; i++)
        most = bits(v[i].q) > bits(v[most].q) ? i : most;

    return reciprocal(v[most].q);
}

/* Whether each of v's n corners has a q of 1, a 2D draw's: its w 1, its texels only scaled. */
static bool flat(const AccelVertex *const v, const uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        if (bits(v[i].q) != ONE)
            return false;

    return true;
}

static float depth_of(const float z) { return (z < 0 ? 0 : z > 1 ? 1 : z) * DEPTH; }

typedef struct {
    float s, t, w;
} Texel;

/* c's texel over w, w scaled by k. */
static Texel texel_of(const AccelVertex *const c, const TdfxDraw *const d, const float k)
{
    const float w = c->q * k, texels = of_bits(scaled(bits(w), d->level));

    return (Texel){c->s * texels, c->t * texels, w};
}

uint32_t tdfx_sent(const uint32_t params)
{
    return CORNERS | params << TDFX_START | params << TDFX_DX | params << TDFX_DY |
           1u << TDFX_COMMAND;
}

uint32_t tdfx_started_sent(const uint32_t params)
{
    return CORNERS | params << TDFX_START | 1u << TDFX_COMMAND;
}

void tdfx_cornered(
    const AccelVertex *const v,
    const uint32_t n,
    const TdfxDraw *const d,
    TdfxCorner *out
)
{
    const uint32_t params = d->params;
    const bool texel = params & TEXEL, even = texel && flat(v, n);
    const float k = texel && !even ? w_scale(v, n) : 1;
    const bool moved = (bits(d->ox) | bits(d->oy)) << 1; /* told by the bits: -0 is 0 */

    for (uint32_t i = 0; i < n; i++, out++) {
        const AccelVertex *const c = &v[i];

        out->x = moved ? bits(c->x + d->ox) : bits(c->x);
        out->y = moved ? bits(c->y + d->oy) : bits(c->y);

        if (params & COLOUR) {
            out->p[TDFX_R] = bits((float)(c->colour >> 16 & CHANNEL));
            out->p[TDFX_G] = bits((float)(c->colour >> 8 & CHANNEL));
            out->p[TDFX_B] = bits((float)(c->colour & CHANNEL));
        }

        if (params & 1u << TDFX_Z)
            out->p[TDFX_Z] = bits(depth_of(c->z));

        if (params & 1u << TDFX_A)
            out->p[TDFX_A] = bits((float)((c->colour >> 24) * d->share) * FADE);

        if (even) {
            out->p[TDFX_S] = scaled(bits(c->s), d->level);
            out->p[TDFX_T] = scaled(bits(c->t), d->level);
            out->p[TDFX_W] = ONE;
        } else if (texel) {
            const Texel t = texel_of(c, d, k);

            out->p[TDFX_S] = bits(t.s);
            out->p[TDFX_T] = bits(t.t);
            out->p[TDFX_W] = bits(t.w);
        }
    }
}

/* A float's bits, ordered as it is when compared as integers: Glide's sort, off a 486's slow
   compares. */
static int32_t ordered(const uint32_t b)
{
    return (int32_t)b < 0 ? (int32_t)(b ^ 0x7FFFFFFF) : (int32_t)b;
}

/* a, b and c sorted top first, into t: true if an odd number of swaps did it. */
static bool sorted(
    const TdfxCorner *a,
    const TdfxCorner *b,
    const TdfxCorner *c,
    const TdfxCorner *t[3]
)
{
    const TdfxCorner *swap;
    bool odd = false;

    if (ordered(b->y) < ordered(a->y))
        swap = a, a = b, b = swap, odd = !odd;

    if (ordered(c->y) < ordered(b->y))
        swap = b, b = c, c = swap, odd = !odd;

    if (ordered(b->y) < ordered(a->y))
        swap = a, a = b, b = swap, odd = !odd;

    t[0] = a, t[1] = b, t[2] = c;
    return odd;
}

/* t's corners and each parameter's start, A's: the words every triangle sends. */
static void started(const TdfxCorner *const t[3], const uint32_t params, TdfxTriangle *const out)
{
    for (uint32_t i = 0; i < 3; i++) {
        out->word[2 * i] = t[i]->x;
        out->word[2 * i + 1] = t[i]->y;
    }

#pragma clang loop unroll(full) /* each parameter's places constant */
    for (uint32_t n = 0; n < TDFX_PARAMS; n++)
        if (params >> n & 1)
            out->word[TDFX_START + n] = t[0]->p[n];
}

bool tdfx_triangle(
    const TdfxCorner *const a,
    const TdfxCorner *const b,
    const TdfxCorner *const c,
    const uint32_t params,
    TdfxTriangle *const out
)
{
    const TdfxCorner *t[3];
    const bool odd = sorted(a, b, c, t);
    const float x0 = of_bits(t[0]->x), y0 = of_bits(t[0]->y);
    const float x1 = of_bits(t[1]->x) - x0, x2 = of_bits(t[2]->x) - x0;
    const float y1 = of_bits(t[1]->y) - y0, y2 = of_bits(t[2]->y) - y0;
    const uint32_t area = bits(x1 * y2 - x2 * y1);

    if (!(area << 1))
        return false;

    const float per = reciprocal(of_bits(area));
    const float xb = y2 * per, xc = y1 * per, yb = x2 * per, yc = x1 * per;

    started(t, params, out);

#pragma clang loop unroll(full) /* each parameter's places constant */
    for (uint32_t n = 0; n < TDFX_PARAMS; n++) {
        if (!(params >> n & 1))
            continue;

        const uint32_t at = t[0]->p[n];

        /* alike at each corner, as a sprite's tint, depth and q are: without the FPU */
        if (t[1]->p[n] == at && t[2]->p[n] == at) {
            out->word[TDFX_DX + n] = out->word[TDFX_DY + n] = 0;
            continue;
        }

        const float p0 = of_bits(at), pb = of_bits(t[1]->p[n]) - p0, pc = of_bits(t[2]->p[n]) - p0;

        out->word[TDFX_DX + n] = bits(pb * xb - pc * xc);
        out->word[TDFX_DY + n] = bits(pc * yc - pb * yb);
    }

    /* the card's own area, the same: B left of A to C, negative */
    out->word[TDFX_COMMAND] = area & SIGN;
    out->turn = (area ^ (odd ? SIGN : 0)) & SIGN;
    return true;
}

void tdfx_started(
    const TdfxCorner *const a,
    const TdfxCorner *const b,
    const TdfxCorner *const c,
    const uint32_t params,
    const uint32_t turn,
    TdfxTriangle *const out
)
{
    const TdfxCorner *t[3];
    const bool odd = sorted(a, b, c, t);

    started(t, params, out);
    out->word[TDFX_COMMAND] = turn ^ (odd ? SIGN : 0);
}

/* Whether, of floats' bits p0 to p3, 3 is where the plane of 0, 1 and 2 puts it, as a
   parallelogram's is: exactly, alike along two opposite sides, as a rectangle of texels and an
   unturned sprite's corners are; else 0 and 2 summed as 1 and 3, within 2 to the -CLOSE of the
   largest, many times the floats' rounding. */
static bool parallel(const uint32_t p0, const uint32_t p1, const uint32_t p2, const uint32_t p3)
{
    enum : uint32_t { CLOSE = 18, MAGNITUDE = 0x7FFFFFFF, EXPONENT = 23 };

    if ((p0 == p3 && p1 == p2) || (p0 == p1 && p2 == p3))
        return true;

    const float f0 = of_bits(p0), f1 = of_bits(p1), f2 = of_bits(p2), f3 = of_bits(p3);
    const uint32_t gap = bits(f0 + f2 - (f1 + f3)) & MAGNITUDE;
    const uint32_t a = p0 & MAGNITUDE, b = p1 & MAGNITUDE, c = p2 & MAGNITUDE, d = p3 & MAGNITUDE;
    const uint32_t ab = a > b ? a : b, cd = c > d ? c : d, most = ab > cd ? ab : cd;

    return !gap || (gap >> EXPONENT) + CLOSE <= most >> EXPONENT;
}

bool tdfx_planar(const TdfxCorner c[4], const uint32_t params)
{
    if (!parallel(c[0].x, c[1].x, c[2].x, c[3].x) || !parallel(c[0].y, c[1].y, c[2].y, c[3].y))
        return false;

#pragma clang loop unroll(full) /* each parameter's places constant */
    for (uint32_t n = 0; n < TDFX_PARAMS; n++)
        if (params >> n & 1 && !parallel(c[0].p[n], c[1].p[n], c[2].p[n], c[3].p[n]))
            return false;

    return true;
}

bool tdfx_setup(const AccelVertex v[3], const TdfxDraw *const d, TdfxTriangle *const out)
{
    TdfxCorner c[3];

    tdfx_cornered(v, 3, d, c);
    return tdfx_triangle(&c[0], &c[1], &c[2], d->params, out);
}

/* The colour or alpha sent packed, ARGB a word: the unit then works out the colour too. */
uint32_t tdfx_unit_mode(const uint32_t params)
{
    return (params & (COLOUR | 1u << TDFX_A) ? SST_UNIT_RGB : 0) |
           (params & 1u << TDFX_A ? SST_UNIT_ALPHA : 0) | (params & 1u << TDFX_Z ? SST_UNIT_Z : 0) |
           (params & TEXEL ? SST_UNIT_W0 | SST_UNIT_ST0 : 0);
}

uint32_t tdfx_corner_sent(const uint32_t params)
{
    return 1u << TDFX_VX | 1u << TDFX_VY |
           (params & (COLOUR | 1u << TDFX_A) ? 1u << TDFX_VARGB : 0) |
           (params & 1u << TDFX_Z ? 1u << TDFX_VZ : 0) |
           (params & TEXEL ? 1u << TDFX_VW | 1u << TDFX_VS | 1u << TDFX_VT : 0);
}

/* a * share / 255 rounded down, by shifts: exact for every pair. */
static uint32_t shared(const uint32_t a, const uint8_t share)
{
    const uint32_t x = a * share;

    return share == CHANNEL ? a : (x + 1 + (x >> 8)) >> 8;
}

uint32_t tdfx_argb(const uint32_t colour, const uint8_t share)
{
    return (colour & RGB) | shared(colour >> 24, share) << 24;
}

void tdfx_corners(
    const AccelVertex *const v,
    const uint32_t n,
    const TdfxDraw *const d,
    uint32_t *out
)
{
    const uint32_t params = d->params;
    const bool even = params & TEXEL && flat(v, n);
    const bool argb = params & (COLOUR | 1u << TDFX_A);
    const float k = params & TEXEL && !even ? w_scale(v, n) : 1;
    /* only a screen centred in a larger mode: else x and y as they are, a 486's add 20 cycles */
    const bool moved = d->ox != 0 || d->oy != 0;

    for (uint32_t i = 0; i < n; i++) {
        const AccelVertex *const c = &v[i];

        *out++ = moved ? bits(c->x + d->ox) : bits(c->x);
        *out++ = moved ? bits(c->y + d->oy) : bits(c->y);

        if (argb)
            *out++ = tdfx_argb(c->colour, d->share);

        if (params & 1u << TDFX_Z)
            *out++ = bits(depth_of(c->z));

        if (params & TEXEL && even) {
            *out++ = ONE;
            *out++ = scaled(bits(c->s), d->level);
            *out++ = scaled(bits(c->t), d->level);
        } else if (params & TEXEL) {
            const Texel t = texel_of(c, d, k);

            *out++ = bits(t.w);
            *out++ = bits(t.s);
            *out++ = bits(t.t);
        }
    }
}

TdfxWords tdfx_words(const AccelState *const s, const uint32_t draw, const bool indexed)
{
    const bool texel = s->source != ACCEL_COLOUR, test = s->depth & ACCEL_DEPTH_TEST;
    const bool keyed = texel && s->flags & ACCEL_KEY;
    const uint32_t mask = indexed ? SST_PATH_INDEX_MASK : SST_PATH_MASK;
    const uint32_t path = s->source == ACCEL_MASK    ? mask
                          : !texel                   ? SST_PATH_COLOUR
                          : s->flags & ACCEL_REPLACE ? SST_PATH_REPLACE
                                                     : SST_PATH_TEXEL;
    const uint32_t format =
        indexed ? SST_P8 << SST_FORMAT | SST_SEQUENTIAL : SST_ARGB1555 << SST_FORMAT;

    TdfxWords w = {
        .colour_path = path | SST_PATH_ADJUST,
        .alpha_mode = s->blend == ACCEL_MIX   ? SST_MIX
                      : s->blend == ACCEL_ADD ? SST_ADD
                                              : 0,
        .fbz_mode = SST_CLIPPED | draw << SST_DRAW_BUFFER |
                    (s->blend != ACCEL_KEEP ? SST_RGB_WRITE : 0) |
                    (keyed && indexed ? SST_CHROMA : 0),
        .texture_mode = SST_PERSPECTIVE | format | SST_LOCAL |
                        (s->flags & ACCEL_LINEAR ? SST_LINEAR : 0) |
                        (s->flags & ACCEL_WRAP_S ? 0 : SST_CLAMP_S) |
                        (s->flags & ACCEL_WRAP_T ? 0 : SST_CLAMP_T),
    };

    if (keyed && !indexed)
        w.alpha_mode |= SST_ALPHA_TEST | SST_GREATER << SST_ALPHA_FUNCTION;

    if (s->depth)
        w.fbz_mode |= SST_DEPTH | (test ? SST_GREATER : SST_ALWAYS) << SST_DEPTH_FUNCTION |
                      (s->depth & ACCEL_DEPTH_WRITE ? SST_AUX_WRITE : 0);

    return w;
}

uint32_t tdfx_level(const uint16_t w, const uint16_t h) { return 8 - log2_of(w > h ? w : h); }

static uint32_t aspect_of(const uint16_t w, const uint16_t h)
{
    return w > h ? log2_of(w / h) : log2_of(h / w);
}

uint32_t tdfx_lod(const uint16_t w, const uint16_t h, const uint32_t levels)
{
    const uint32_t level = LEVEL * tdfx_level(w, h), last = level + LEVEL * (levels - 1);
    const uint32_t bias = levels > 1 ? LEVEL / 2 : 0;

    return level << SST_LOD_MIN | last << SST_LOD_MAX | bias << SST_LOD_BIAS |
           (w > h ? SST_S_WIDER : 0) | aspect_of(w, h) << SST_ASPECT;
}

uint32_t tdfx_chain(const uint16_t w, const uint16_t h, const uint32_t levels, const uint32_t texel)
{
    uint32_t bytes = 0;

    for (uint32_t k = 0; k < levels; k++)
        bytes += (uint32_t)(w >> k) * (uint32_t)(h >> k) * texel;

    return bytes;
}

uint32_t tdfx_skipped(const uint16_t w, const uint16_t h, const uint32_t texel)
{
    const uint32_t aspect = aspect_of(w, h);

    uint32_t bytes = 0;

    for (uint32_t k = 0; k < tdfx_level(w, h); k++) {
        const uint32_t longer = SIDE >> k, shorter = longer >> aspect;
        bytes += longer * (shorter ? shorter : 1) * texel;
    }

    return bytes;
}
