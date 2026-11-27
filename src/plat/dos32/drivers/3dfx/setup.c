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

/* The colour unless the texel alone or no colour written; alpha for a blend or the key's test. */
uint32_t tdfx_params(const AccelState *const s, const bool texel)
{
    const bool alone = texel && s->source == ACCEL_TEXEL && s->flags & ACCEL_REPLACE;
    const bool alpha = s->blend == ACCEL_MIX || s->blend == ACCEL_ADD || s->flags & ACCEL_KEY;

    return (alone || s->blend == ACCEL_KEEP ? 0 : COLOUR) | (alone || !alpha ? 0 : 1u << TDFX_A) |
           (s->depth ? 1u << TDFX_Z : 0) | (texel ? TEXEL : 0);
}

/* W's scale for v's n corners: the largest q made 1, a 2D draw's q of 1 left undivided. */
static float w_scale(const AccelVertex *const v, const uint32_t n)
{
    float most = v[0].q;

    for (uint32_t i = 1; i < n; i++)
        most = v[i].q > most ? v[i].q : most;

    return reciprocal(most);
}

static float depth_of(const float z) { return (z < 0 ? 0 : z > 1 ? 1 : z) * DEPTH; }

typedef struct {
    float s, t, w;
} Texel;

/* c's texel over w, w scaled by k. */
static Texel texel_of(const AccelVertex *const c, const TdfxDraw *const d, const float k)
{
    const float w = c->q * k, texels = d->scale * w;

    return (Texel){c->s * texels, c->t * texels, w};
}

/* The parameters of params alike at v's corners, their gradients 0: told by the bits, without the
   FPU. */
static uint32_t alike(const AccelVertex v[3], const uint32_t params)
{
    const uint32_t differ = (v[0].colour ^ v[1].colour) | (v[0].colour ^ v[2].colour);
    const uint32_t z = bits(v[0].z), q = bits(v[0].q);
    const bool flat = z == bits(v[1].z) && z == bits(v[2].z);
    const bool even = q == bits(v[1].q) && q == bits(v[2].q);

    return params & ((differ & RGB ? 0 : COLOUR) | (differ >> 24 ? 0 : 1u << TDFX_A) |
                     (flat ? 1u << TDFX_Z : 0) | (even ? 1u << TDFX_W : 0));
}

/* B's and C's differences from A weighed in each gradient: a parameter's are its own times these. */
typedef struct {
    float xb, xc, yb, yc;
} Weights;

/* Parameters first to last of at sent: their starts, and their gradients, 0 if alike. Inlined at
   each group, so that each group's alike is its own branch, as steady as the draws. */
static inline void graded(
    TdfxTriangle *const out,
    const uint32_t first,
    const uint32_t last,
    const bool alike,
    const float at[3][TDFX_PARAMS],
    const Weights *const w
)
{
    for (uint32_t n = first; n <= last; n++) {
        out->word[TDFX_START + n] = bits(at[0][n]);

        if (alike) {
            out->word[TDFX_DX + n] = out->word[TDFX_DY + n] = 0;
            continue;
        }

        const float b = at[1][n] - at[0][n], c = at[2][n] - at[0][n];

        out->word[TDFX_DX + n] = bits(b * w->xb - c * w->xc);
        out->word[TDFX_DY + n] = bits(c * w->yc - b * w->yb);
    }
}

uint32_t tdfx_sent(const uint32_t params)
{
    return CORNERS | params << TDFX_START | params << TDFX_DX | params << TDFX_DY |
           1u << TDFX_COMMAND;
}

bool tdfx_setup(const AccelVertex v[3], const TdfxDraw *const d, TdfxTriangle *const out)
{
    const AccelVertex *p[3] = {&v[0], &v[1], &v[2]};
    const AccelVertex *swap;

    if (p[1]->y < p[0]->y)
        swap = p[0], p[0] = p[1], p[1] = swap;

    if (p[2]->y < p[1]->y)
        swap = p[1], p[1] = p[2], p[2] = swap;

    if (p[1]->y < p[0]->y)
        swap = p[0], p[0] = p[1], p[1] = swap;

    const uint32_t params = d->params, still = alike(v, params);
    const float k = params & TEXEL ? w_scale(v, 3) : 1;

    float x[3], y[3], at[3][TDFX_PARAMS];

    for (uint32_t i = 0; i < 3; i++) {
        const AccelVertex *const c = p[i];
        const uint32_t need = i ? params & ~still : params; /* the alike only at A */

        x[i] = c->x + 0.5f + d->ox;
        y[i] = c->y + 0.5f + d->oy;

        if (need & COLOUR) {
            at[i][TDFX_R] = (float)(c->colour >> 16 & CHANNEL);
            at[i][TDFX_G] = (float)(c->colour >> 8 & CHANNEL);
            at[i][TDFX_B] = (float)(c->colour & CHANNEL);
        }

        if (need & 1u << TDFX_Z)
            at[i][TDFX_Z] = depth_of(c->z);

        if (need & 1u << TDFX_A)
            at[i][TDFX_A] = (float)((c->colour >> 24) * d->share) * FADE;

        if (need & TEXEL) {
            const Texel t = texel_of(c, d, k);

            at[i][TDFX_S] = t.s;
            at[i][TDFX_T] = t.t;
            at[i][TDFX_W] = t.w;
        }
    }

    const float x1 = x[1] - x[0], x2 = x[2] - x[0], y1 = y[1] - y[0], y2 = y[2] - y[0];
    const float area = x1 * y2 - x2 * y1;

    if (area == 0)
        return false;

    const float per = 1 / area;
    const Weights w = {y2 * per, y1 * per, x2 * per, x1 * per};

    if (params & COLOUR)
        graded(out, TDFX_R, TDFX_B, still & COLOUR, at, &w);

    if (params & 1u << TDFX_Z)
        graded(out, TDFX_Z, TDFX_Z, still >> TDFX_Z & 1, at, &w);

    if (params & 1u << TDFX_A)
        graded(out, TDFX_A, TDFX_A, still >> TDFX_A & 1, at, &w);

    if (params & TEXEL) {
        graded(out, TDFX_S, TDFX_T, false, at, &w);
        graded(out, TDFX_W, TDFX_W, still >> TDFX_W & 1, at, &w);
    }

    for (uint32_t i = 0; i < 3; i++) {
        out->word[2 * i] = bits(x[i]);
        out->word[2 * i + 1] = bits(y[i]);
    }

    /* the card's own area, the same: B left of A to C, negative */
    out->word[TDFX_COMMAND] = area < 0 ? SIGN : 0;
    return true;
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

/* Whether t's three corners are colour, as read reads them. */
static bool all(const AccelVertex t[3], const uint32_t read, const uint32_t colour)
{
    return (t[0].colour & read) == colour && (t[1].colour & read) == colour &&
           (t[2].colour & read) == colour;
}

uint32_t tdfx_run(
    const AccelVertex *const v,
    const uint32_t n,
    const uint32_t params,
    bool *const one
)
{
    const uint32_t read = (params & COLOUR ? RGB : 0) | (params & 1u << TDFX_A ? ~RGB : 0);
    const uint32_t first = v[0].colour & read;
    const bool same = read && all(v, read, first);

    uint32_t k = read ? 1 : n;

    if (same)
        while (k < n && all(v + 3 * k, read, first))
            k++;
    else
        while (k < n && !all(v + 3 * k, read, v[3 * k].colour & read))
            k++;

    *one = same;
    return k;
}

void tdfx_corners(
    const AccelVertex *const v,
    const uint32_t n,
    const TdfxDraw *const d,
    TdfxVertex *const out
)
{
    const uint32_t params = d->params;
    const float k = params & TEXEL ? w_scale(v, n) : 1;

    for (uint32_t i = 0; i < n; i++) {
        const AccelVertex *const c = &v[i];
        uint32_t *const w = out[i].word;

        w[TDFX_VX] = bits(c->x + 0.5f + d->ox);
        w[TDFX_VY] = bits(c->y + 0.5f + d->oy);
        w[TDFX_VARGB] = tdfx_argb(c->colour, d->share);
        w[TDFX_VDRAW] = w[TDFX_VBEGIN] = 0;

        if (params & 1u << TDFX_Z)
            w[TDFX_VZ] = bits(depth_of(c->z));

        if (params & TEXEL) {
            const Texel t = texel_of(c, d, k);

            w[TDFX_VW] = bits(t.w);
            w[TDFX_VS] = bits(t.s);
            w[TDFX_VT] = bits(t.t);
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

uint32_t tdfx_lod(const uint16_t w, const uint16_t h)
{
    const uint32_t level = LEVEL * tdfx_level(w, h);

    return level << SST_LOD_MIN | level << SST_LOD_MAX | (w > h ? SST_S_WIDER : 0) |
           aspect_of(w, h) << SST_ASPECT;
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
