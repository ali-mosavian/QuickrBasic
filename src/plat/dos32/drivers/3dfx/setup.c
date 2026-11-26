/* A triangle and its state as an SST card takes them (setup.h). */
#include "plat/dos32/drivers/3dfx/setup.h"
#include "plat/dos32/drivers/3dfx/regs.h"

enum { CHANNEL = 255, DEPTH = 65535, SIDE = 256, LEVEL = 4, SIGN = 1u << 31 };

static uint32_t log2_of(uint32_t n)
{
    uint32_t l = 0;

    while (n > 1) {
        n >>= 1;
        l++;
    }

    return l;
}

bool tdfx_setup(
    const AccelVertex v[3],
    const float ox,
    const float oy,
    const float scale,
    const uint8_t share,
    TdfxTriangle *const out
)
{
    const AccelVertex *p[3] = {&v[0], &v[1], &v[2]};
    const AccelVertex *swap;

    if (p[1]->y < p[0]->y)
        swap = p[0], p[0] = p[1], p[1] = swap;

    if (p[2]->y < p[1]->y)
        swap = p[1], p[1] = p[2], p[2] = swap;

    if (p[1]->y < p[0]->y)
        swap = p[0], p[0] = p[1], p[1] = swap;

    float most = p[0]->q;

    for (uint32_t i = 1; i < 3; i++)
        most = p[i]->q > most ? p[i]->q : most;

    const float k = 1 / most;

    float at[3][TDFX_PARAMS];

    for (uint32_t i = 0; i < 3; i++) {
        const AccelVertex *const c = p[i];
        const float z = c->z < 0 ? 0 : c->z > 1 ? 1 : c->z, w = c->q * k;

        out->x[i] = c->x + 0.5f + ox;
        out->y[i] = c->y + 0.5f + oy;
        at[i][TDFX_R] = (float)(c->colour >> 16 & CHANNEL);
        at[i][TDFX_G] = (float)(c->colour >> 8 & CHANNEL);
        at[i][TDFX_B] = (float)(c->colour & CHANNEL);
        at[i][TDFX_A] = (float)((c->colour >> 24) * share) / CHANNEL;
        at[i][TDFX_Z] = z * DEPTH;
        at[i][TDFX_S] = c->s * scale * w;
        at[i][TDFX_T] = c->t * scale * w;
        at[i][TDFX_W] = w;
    }

    const float *const x = out->x, *const y = out->y;
    const float area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);

    if (area == 0)
        return false;

    for (uint32_t n = 0; n < TDFX_PARAMS; n++) {
        const float b = at[1][n] - at[0][n], c = at[2][n] - at[0][n];

        out->start[n] = at[0][n];
        out->dx[n] = (b * (y[2] - y[0]) - c * (y[1] - y[0])) / area;
        out->dy[n] = (c * (x[1] - x[0]) - b * (x[2] - x[0])) / area;
    }

    /* the card's own area: B left of A to C, negative */
    const float card = (x[0] - x[1]) * (y[1] - y[2]) - (x[1] - x[2]) * (y[0] - y[1]);

    out->command = card < 0 ? SIGN : 0;
    return true;
}

TdfxWords tdfx_words(const AccelState *const s, const uint32_t draw, const bool indexed)
{
    const bool texel = s->source != ACCEL_COLOUR, test = s->depth & ACCEL_DEPTH_TEST;
    const bool keyed = texel && s->flags & ACCEL_KEY;
    const uint32_t mask = indexed ? SST_PATH_INDEX_MASK : SST_PATH_MASK;
    const uint32_t path = s->source == ACCEL_MASK         ? mask
                          : !texel                        ? 0
                          : s->flags & ACCEL_REPLACE      ? SST_PATH_REPLACE
                                                          : SST_PATH_TEXEL;
    const uint32_t format = indexed ? SST_P8 << SST_FORMAT | SST_SEQUENTIAL : SST_ARGB1555 << SST_FORMAT;

    TdfxWords w = {
        .colour_path = path | SST_PATH_ADJUST,
        .alpha_mode = s->blend == ACCEL_MIX ? SST_MIX : s->blend == ACCEL_ADD ? SST_ADD : 0,
        .fbz_mode = SST_CLIPPED | draw << SST_DRAW_BUFFER | (s->blend != ACCEL_KEEP ? SST_RGB_WRITE : 0) |
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

uint32_t tdfx_indices(const uint16_t *const texels)
{
    return (texels[0] & 0xFFu) | (texels[1] & 0xFFu) << 8 | (texels[2] & 0xFFu) << 16 |
           (uint32_t)(texels[3] & 0xFF) << 24;
}
